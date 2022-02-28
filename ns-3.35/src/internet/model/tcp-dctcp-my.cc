/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Guanzhou Hu @ UW-Madison, CS740, Spring 2022
 *
 * This file contains my own minimal implementation of DCTCP protocol.
 *
 */

#include "tcp-dctcp-my.h"
#include "ns3/log.h"
#include "ns3/abort.h"
#include "ns3/tcp-socket-state.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpDctcpMy");

NS_OBJECT_ENSURE_REGISTERED (TcpDctcpMy);

TypeId TcpDctcpMy::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpDctcpMy")
    .SetParent<TcpLinuxReno> ()
    .AddConstructor<TcpDctcpMy> ()
    .SetGroupName ("Internet")
    .AddAttribute ("DctcpShiftG",
                   "Parameter G for updating dctcp_alpha",
                   DoubleValue (0.0625),
                   MakeDoubleAccessor (&TcpDctcpMy::m_g),
                   MakeDoubleChecker<double> (0, 1))
    .AddAttribute ("DctcpAlphaOnInit",
                   "Initial alpha value",
                   DoubleValue (1.0),
                   MakeDoubleAccessor (&TcpDctcpMy::InitializeDctcpAlpha),
                   MakeDoubleChecker<double> (0, 1))
    .AddAttribute ("UseEct0",
                   "Use ECT(0) for ECN codepoint, if false use ECT(1)",
                   BooleanValue (true),
                   MakeBooleanAccessor (&TcpDctcpMy::m_useEct0),
                   MakeBooleanChecker ())
    .AddTraceSource ("CongestionEstimate",
                     "Update sender-side congestion estimate state",
                     MakeTraceSourceAccessor (&TcpDctcpMy::m_traceCongestionEstimate),
                     "ns3::TcpDctcpMy::CongestionEstimateTracedCallback")
  ;
  return tid;
}

std::string TcpDctcpMy::GetName () const
{
  return "TcpDctcpMy";
}

TcpDctcpMy::TcpDctcpMy ()
  : TcpLinuxReno (),
    m_ackedBytesEcn (0),
    m_ackedBytesTotal (0),
    m_priorRcvNxt (SequenceNumber32 (0)),
    m_priorRcvNxtFlag (false),
    m_nextSeq (SequenceNumber32 (0)),
    m_nextSeqFlag (false),
    m_ceState (false),
    m_delayedAckReserved (false),
    m_initialized (false)
{
  NS_LOG_FUNCTION (this);
}

TcpDctcpMy::TcpDctcpMy (const TcpDctcpMy& sock)
  : TcpLinuxReno (sock),
    m_ackedBytesEcn (sock.m_ackedBytesEcn),
    m_ackedBytesTotal (sock.m_ackedBytesTotal),
    m_priorRcvNxt (sock.m_priorRcvNxt),
    m_priorRcvNxtFlag (sock.m_priorRcvNxtFlag),
    m_alpha (sock.m_alpha),
    m_nextSeq (sock.m_nextSeq),
    m_nextSeqFlag (sock.m_nextSeqFlag),
    m_ceState (sock.m_ceState),
    m_delayedAckReserved (sock.m_delayedAckReserved),
    m_g (sock.m_g),
    m_useEct0 (sock.m_useEct0),
    m_initialized (sock.m_initialized)
{
  NS_LOG_FUNCTION (this);
}

TcpDctcpMy::~TcpDctcpMy (void)
{
  NS_LOG_FUNCTION (this);
}

Ptr<TcpCongestionOps> TcpDctcpMy::Fork (void)
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
  tcb->m_ectCodePoint = m_useEct0 ? TcpSocketState::Ect0 : TcpSocketState::Ect1;
  m_initialized = true;
}

// Step 9, Section 3.3 of RFC 8257.  GetSsThresh() is called upon
// entering the CWR state, and then later, when CWR is exited,
// cwnd is set to ssthresh (this value).  bytesInFlight is ignored.
uint32_t
TcpDctcpMy::GetSsThresh (Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight)
{
  NS_LOG_FUNCTION (this << tcb << bytesInFlight);
  return static_cast<uint32_t> ((1 - m_alpha / 2.0) * tcb->m_cWnd);
}

void
TcpDctcpMy::PktsAcked (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked, const Time &rtt)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked << rtt);
  m_ackedBytesTotal += segmentsAcked * tcb->m_segmentSize;
  if (tcb->m_ecnState == TcpSocketState::ECN_ECE_RCVD)
    {
      m_ackedBytesEcn += segmentsAcked * tcb->m_segmentSize;
    }
  if (m_nextSeqFlag == false)
    {
      m_nextSeq = tcb->m_nextTxSequence;
      m_nextSeqFlag = true;
    }
  if (tcb->m_lastAckedSeq >= m_nextSeq)
    {
      double bytesEcn = 0.0; // Corresponds to variable M in RFC 8257
      if (m_ackedBytesTotal >  0)
        {
          bytesEcn = static_cast<double> (m_ackedBytesEcn * 1.0 / m_ackedBytesTotal);
        }
      m_alpha = (1.0 - m_g) * m_alpha + m_g * bytesEcn;
      m_traceCongestionEstimate (m_ackedBytesEcn, m_ackedBytesTotal, m_alpha);
      NS_LOG_INFO (this << "bytesEcn " << bytesEcn << ", m_alpha " << m_alpha);
      Reset (tcb);
    }
}

void
TcpDctcpMy::InitializeDctcpAlpha (double alpha)
{
  NS_LOG_FUNCTION (this << alpha);
  NS_ABORT_MSG_IF (m_initialized, "DCTCP has already been initialized");
  m_alpha = alpha;
}

void
TcpDctcpMy::Reset (Ptr<TcpSocketState> tcb)
{
  NS_LOG_FUNCTION (this << tcb);
  m_nextSeq = tcb->m_nextTxSequence;
  m_ackedBytesEcn = 0;
  m_ackedBytesTotal = 0;
}

void
TcpDctcpMy::CeState0to1 (Ptr<TcpSocketState> tcb)
{
  NS_LOG_FUNCTION (this << tcb);
  if (!m_ceState && m_delayedAckReserved && m_priorRcvNxtFlag)
    {
      SequenceNumber32 tmpRcvNxt;
      /* Save current NextRxSequence. */
      tmpRcvNxt = tcb->m_rxBuffer->NextRxSequence ();

      /* Generate previous ACK without ECE */
      tcb->m_rxBuffer->SetNextRxSequence (m_priorRcvNxt);
      tcb->m_sendEmptyPacketCallback (TcpHeader::ACK);

      /* Recover current RcvNxt. */
      tcb->m_rxBuffer->SetNextRxSequence (tmpRcvNxt);
    }

  if (m_priorRcvNxtFlag == false)
    {
      m_priorRcvNxtFlag = true;
    }
  m_priorRcvNxt = tcb->m_rxBuffer->NextRxSequence ();
  m_ceState = true;
  tcb->m_ecnState = TcpSocketState::ECN_CE_RCVD;
}

void
TcpDctcpMy::CeState1to0 (Ptr<TcpSocketState> tcb)
{
  NS_LOG_FUNCTION (this << tcb);
  if (m_ceState && m_delayedAckReserved && m_priorRcvNxtFlag)
    {
      SequenceNumber32 tmpRcvNxt;
      /* Save current NextRxSequence. */
      tmpRcvNxt = tcb->m_rxBuffer->NextRxSequence ();

      /* Generate previous ACK with ECE */
      tcb->m_rxBuffer->SetNextRxSequence (m_priorRcvNxt);
      tcb->m_sendEmptyPacketCallback (TcpHeader::ACK | TcpHeader::ECE);

      /* Recover current RcvNxt. */
      tcb->m_rxBuffer->SetNextRxSequence (tmpRcvNxt);
    }

  if (m_priorRcvNxtFlag == false)
    {
      m_priorRcvNxtFlag = true;
    }
  m_priorRcvNxt = tcb->m_rxBuffer->NextRxSequence ();
  m_ceState = false;

  if (tcb->m_ecnState == TcpSocketState::ECN_CE_RCVD || tcb->m_ecnState == TcpSocketState::ECN_SENDING_ECE)
    {
      tcb->m_ecnState = TcpSocketState::ECN_IDLE;
    }
}

void
TcpDctcpMy::UpdateAckReserved (Ptr<TcpSocketState> tcb,
                             const TcpSocketState::TcpCAEvent_t event)
{
  NS_LOG_FUNCTION (this << tcb << event);
  switch (event)
    {
    case TcpSocketState::CA_EVENT_DELAYED_ACK:
      if (!m_delayedAckReserved)
        {
          m_delayedAckReserved = true;
        }
      break;
    case TcpSocketState::CA_EVENT_NON_DELAYED_ACK:
      if (m_delayedAckReserved)
        {
          m_delayedAckReserved = false;
        }
      break;
    default:
      /* Don't care for the rest. */
      break;
    }
}

void
TcpDctcpMy::CwndEvent (Ptr<TcpSocketState> tcb,
                     const TcpSocketState::TcpCAEvent_t event)
{
  NS_LOG_FUNCTION (this << tcb << event);
  switch (event)
    {
    case TcpSocketState::CA_EVENT_ECN_IS_CE:
      CeState0to1 (tcb);
      break;
    case TcpSocketState::CA_EVENT_ECN_NO_CE:
      CeState1to0 (tcb);
      break;
    case TcpSocketState::CA_EVENT_DELAYED_ACK:
    case TcpSocketState::CA_EVENT_NON_DELAYED_ACK:
      UpdateAckReserved (tcb, event);
      break;
    default:
      /* Don't care for the rest. */
      break;
    }
}

} // namespace ns3
