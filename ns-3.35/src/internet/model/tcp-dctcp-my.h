/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Guanzhou Hu @ UW-Madison, CS740, Spring 2022
 *
 * This file is a duplicate of `tcp-dctcp.h`.
 *
 */

#ifndef TCP_DCTCP_MY_H
#define TCP_DCTCP_MY_H

#include "ns3/tcp-congestion-ops.h"
#include "ns3/tcp-linux-reno.h"
#include "ns3/traced-callback.h"

namespace ns3 {

/**
 * \ingroup tcp
 *
 * \brief An minimal implementation of DCTCP according to the SIGCOMM paper.
 */
class TcpDctcpMy : public TcpLinuxReno
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  /**
   * Create an unbound tcp socket.
   */
  TcpDctcpMy ();

  /**
   * \brief Copy constructor
   * \param sock the object to copy
   */
  TcpDctcpMy (const TcpDctcpMy& sock);

  /**
   * \brief Destructor
   */
  virtual ~TcpDctcpMy (void);

  /**
   * \brief Set configuration required by congestion control algorithm,
   *        This method will force DctcpEcn mode and will force usage of
   *        either ECT(0) or ECT(1) (depending on the 'UseEct0' attribute),
   *        despite any other configuration in the base classes.
   *
   * \param tcb internal congestion state
   */
  virtual void Init (Ptr<TcpSocketState> tcb);

  // Documented in base class
  virtual std::string GetName () const;
  virtual Ptr<TcpCongestionOps> Fork ();
  virtual void CwndEvent (Ptr<TcpSocketState> tcb, const TcpSocketState::TcpCAEvent_t event);
  virtual void PktsAcked (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked, const Time &rtt);
  virtual uint32_t GetSsThresh (Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight);

  /**
   * TracedCallback signature for DCTCP update of congestion state
   *
   * \param [in] bytesAcked Bytes acked in this observation window
   * \param [in] bytesMarked Bytes marked in this observation window
   * \param [in] alpha New alpha (congestion estimate) value
   */
  typedef void (* DctcpUpdateCallback)(uint32_t bytesAcked, uint32_t bytesMarked, double alpha);

private:
  void FlushDelayedACK (Ptr<TcpSocketState> tcb, bool setECE);
  void TcpDctcpMy::UpdateDelayedACK (Ptr<TcpSocketState> tcb);
  void CEStateOn (Ptr<TcpSocketState> tcb);
  void CEStateOff (Ptr<TcpSocketState> tcb);

  bool initialized;                 //!< Whether DCTCP has been initialized
  double alpha;                     //!< Current estimate of network congestion
  double g;                         //!< Estimation gain
  bool CEState;                     //!< DCTCP.CE state
  bool holdingDelayedACK;           //!< Is there delayed ACK held
  SequenceNumber32 seqDelayedACK;   //!< Sequence number of first byte whose ACK is held delayed
  bool seqDelayedACKValid;          //!< Is seqDelayedACK valid
  uint32_t bytesACKedAll;           //!< Total number of acked bytes
  uint32_t bytesACKedECE;           //!< Number of acked bytes which are marked ECE
  SequenceNumber32 seqNextSend;     //!< Sequence number of the next byte in tx sequence
  bool seqNextSendValid;            //!< Is seqNextSend valid

  TracedCallback<uint32_t, uint32_t, double> traceDctcpUpdate;
};

} // namespace ns3

#endif /* TCP_DCTCP_MY_H */
