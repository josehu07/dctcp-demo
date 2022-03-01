/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Guanzhou Hu @ UW-Madison, CS740, Spring 2022
 *
 * This file contains my own minimal implementation of DCTCP protocol.
 * Some of the skeleton code is adopted from `tcp-dctcp.cc`.
 *
 * The main DCTCP algorithm (in practice) has been well summarized in
 * section 3 of RFC 8257: https://datatracker.ietf.org/doc/html/rfc8257
 *
 */

#include "tcp-dctcp-my.h"
#include "ns3/log.h"
#include "ns3/abort.h"
#include "ns3/tcp-socket-state.h"


// In ns-3, a congestion control algorithm is implemented by providing a
// set of congestion control operation handlers, declared in the file
// `tcp-congestion-ops.h`. DCTCP is no exception.

// The good thing about DCTCP is that it builds upon standard ECN mechanisms,
// so that for the "routers marking CE bit" part, we don't need to implement
// anything new -- the RED ECN algorithm is capable of doing that and is
// directly usable in ns-3. We only need to code the end-host (sender/receiver)
// logic below.


namespace ns3 {


NS_LOG_COMPONENT_DEFINE ("TcpDctcpMy");
NS_OBJECT_ENSURE_REGISTERED (TcpDctcpMy);


/////////////////////////////////////////
// TcpDctcpMy c++ class common methods //
/////////////////////////////////////////

TypeId TcpDctcpMy::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::TcpDctcpMy")
    .SetParent<TcpLinuxReno> ()
    .AddConstructor<TcpDctcpMy> ()
    .SetGroupName ("Internet")
    .AddAttribute ("DctcpG",
                   "Sliding window weight, g, in a = (1-g) * a + g * F",
                   DoubleValue (0.0625),    // recommended value in RFC 8257
                   MakeDoubleAccessor (&TcpDctcpMy::g),
                   MakeDoubleChecker<double> (0, 1))
    .AddTraceSource ("DctcpUpdate",
                     "Update sender-side congestion estimate variables",
                     MakeTraceSourceAccessor (&TcpDctcpMy::traceDctcpUpdate),
                     "ns3::TcpDctcpMy::DctcpUpdateCallback")
  ;
  return tid;
}

std::string TcpDctcpMy::GetName () const
{
  return "TcpDctcpMy";
}

TcpDctcpMy::TcpDctcpMy ()
  : TcpLinuxReno (),
    initialized (false),
    alpha(1.0),
    // g is provided as an attribute
    CEState (false),
    holdingDelayedACK (false),
    seqDelayedACK (SequenceNumber32 (0)),
    seqDelayedACKValid (false),
    bytesACKedAll (0),
    bytesACKedECE (0),
    seqNextSend (SequenceNumber32 (0)),
    seqNextSendValid (false)
{
  NS_LOG_FUNCTION (this);
}

TcpDctcpMy::TcpDctcpMy (const TcpDctcpMy& sock)
  : TcpLinuxReno (sock),
    initialized (sock.initialized),
    alpha (sock.alpha),
    g (sock.g),
    CEState (sock.CEState),
    holdingDelayedACK (sock.holdingDelayedACK),
    seqDelayedACK (sock.seqDelayedACK),
    seqDelayedACKValid (sock.seqDelayedACKValid),
    bytesACKedAll (sock.bytesACKedAll),
    bytesACKedECE (sock.bytesACKedECE),
    seqNextSend (sock.seqNextSend),
    seqNextSendValid (sock.seqNextSendValid)
{
  NS_LOG_FUNCTION (this);
}

TcpDctcpMy::~TcpDctcpMy ()
{
  NS_LOG_FUNCTION (this);
}

Ptr<TcpCongestionOps> TcpDctcpMy::Fork ()
{
  NS_LOG_FUNCTION (this);
  return CopyObject<TcpDctcpMy> (this);
}

void
TcpDctcpMy::Init (Ptr<TcpSocketState> tcb)
{
  NS_LOG_FUNCTION (this << tcb);
  NS_LOG_INFO (this << "Enabling DctcpEcn for DCTCP");
  tcb->m_useEcn = TcpSocketState::On;
  tcb->m_ecnMode = TcpSocketState::DctcpEcn;
  tcb->m_ectCodePoint = TcpSocketState::Ect0;   // ECT(1) has no meaning so far
  initialized = true;
}


/////////////////////////////////////////
// DCTCP: receiver side implementation //
/////////////////////////////////////////

// Congestion op: what should the receiver do when receiving a congestion
// avoidance event signal.
void
TcpDctcpMy::CwndEvent (Ptr<TcpSocketState> tcb,
                       const TcpSocketState::TcpCAEvent_t event)
{
  NS_LOG_FUNCTION (this << tcb << event);

  // section 2, RFC 8257:
  switch (event) {
    case TcpSocketState::CA_EVENT_DELAYED_ACK:
      // sent a delayed ACK
      holdingDelayedACK = true;
      break;
    case TcpSocketState::CA_EVENT_NON_DELAYED_ACK:
      // sent a non-delayed ACK
      holdingDelayedACK = false;
      break;
    case TcpSocketState::CA_EVENT_ECN_IS_CE:
      // received packet with CE bit 1
      CEStateOn (tcb);
      break;
    case TcpSocketState::CA_EVENT_ECN_NO_CE:
      // received packet with CE bit 0
      CEStateOff (tcb);
      break;
    default:
      // don't care
      break;
  }
}

void
TcpDctcpMy::FlushDelayedACK (Ptr<TcpSocketState> tcb, bool setECE)
{
  if (holdingDelayedACK && seqDelayedACKValid) {
    TcpHeader::Flags_t flags = setECE ? (TcpHeader::ACK | TcpHeader::ECE)
                                      :  TcpHeader::ACK;
    SequenceNumber32 seqCurrent = tcb->m_rxBuffer->NextRxSequence ();
    tcb->m_rxBuffer->SetNextRxSequence (seqDelayedACK);
    tcb->m_sendEmptyPacketCallback (flags);
    tcb->m_rxBuffer->SetNextRxSequence (seqCurrent);
  }
}

void
TcpDctcpMy::UpdateDelayedACK (Ptr<TcpSocketState> tcb)
{
  seqDelayedACK = tcb->m_rxBuffer->NextRxSequence ();
  seqDelayedACKValid = true;
}

void
TcpDctcpMy::CEStateOn (Ptr<TcpSocketState> tcb)
{
  NS_LOG_FUNCTION (this << tcb);

  // if CE state is transitioning from 0 to 1, should send out the ACK
  // without ECE bit for all bytes whose ACK is delayed
  if (!CEState) {
    FlushDelayedACK (tcb, /*setECE*/ false);
    CEState = true;
  }

  // update seqDelayedACK to be this packet
  UpdateDelayedACK (tcb);

  // set tcb's state to CE_RCVD
  tcb->m_ecnState = TcpSocketState::ECN_CE_RCVD;
}

void
TcpDctcpMy::CEStateOff (Ptr<TcpSocketState> tcb)
{
  NS_LOG_FUNCTION (this << tcb);

  // if CE state is transitioning from 1 to 0, should send out the ACK
  // with ECE for all bytes whose ACK is delayed
  if (CEState) {
    FlushDelayedACK (tcb, /*setECE*/ true);
    CEState = false;
  }

  // update seqDelayedACK to be this packet
  UpdateDelayedACK (tcb);

  // set tcb's state to CE_IDLE
  if (tcb->m_ecnState == TcpSocketState::ECN_CE_RCVD ||
      tcb->m_ecnState == TcpSocketState::ECN_SENDING_ECE) {
    tcb->m_ecnState = TcpSocketState::ECN_IDLE;
  }
}


///////////////////////////////////////
// DCTCP: sender side implementation //
///////////////////////////////////////

// Congestion op: what should the sender do when accepting an ACK.
void
TcpDctcpMy::PktsAcked (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked,
                       const Time &rtt)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked << rtt);

  // step 1~3, section 3.3, RFC 8257: accumulate the number of bytes ACKed
  bytesACKedAll += segmentsAcked * tcb->m_segmentSize;
  if (tcb->m_ecnState == TcpSocketState::ECN_ECE_RCVD)
    bytesACKedECE += segmentsAcked * tcb->m_segmentSize;

  // step 4, section 3.3, RFC 8257: update seqNextSend; if reached the end
  // of observation window, proceed to step 5
  if (!seqNextSendValid) {
    seqNextSend = tcb->m_nextTxSequence;
    seqNextSendValid = true;
  }
  if (tcb->m_lastAckedSeq < seqNextSend)
    return;   // not yet the end of current observation window

  // step 5, section 3.3, RFC 8257: compute congestion level
  double fracF = 0.0;
  if (bytesACKedAll > 0)
    fracF = static_cast<double> (bytesACKedECE * 1.0 / bytesACKedAll);

  // step 6, section 3.3, RFC 8257: a = (1-g) * a + g * F
  alpha = (1.0 - g) * alpha + g * fracF;
  traceDctcpUpdate (bytesACKedECE, bytesACKedAll, alpha);
  NS_LOG_INFO (this << "fracF " << fracF << ", alpha " << alpha);

  // step 7~8, section 3.3, RFC 8257: determine the end of the next
  // observation window, reset the byte counters
  seqNextSend = tcb->m_nextTxSequence;
  bytesACKedECE = 0;
  bytesACKedAll = 0;
}

// Congestion op: set window size after a loss event.
uint32_t
TcpDctcpMy::GetSsThresh (Ptr<const TcpSocketState> tcb,
                         uint32_t bytesInFlight)
{
  NS_LOG_FUNCTION (this << tcb << bytesInFlight);

  // step 9, section 3.3, RFC 8257: cwnd = cwnd * (1 - a / 2)
  return static_cast<uint32_t> ((1.0 - alpha / 2.0) * tcb->m_cWnd);
}


} // namespace ns3
