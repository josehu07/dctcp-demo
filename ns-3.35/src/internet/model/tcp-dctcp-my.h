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

  // Documented in base class
  virtual std::string GetName () const;

  /**
   * \brief Set configuration required by congestion control algorithm,
   *        This method will force DctcpEcn mode and will force usage of
   *        either ECT(0) or ECT(1) (depending on the 'UseEct0' attribute),
   *        despite any other configuration in the base classes.
   *
   * \param tcb internal congestion state
   */
  virtual void Init (Ptr<TcpSocketState> tcb);

  /**
   * TracedCallback signature for DCTCP update of congestion state
   *
   * \param [in] bytesAcked Bytes acked in this observation window
   * \param [in] bytesMarked Bytes marked in this observation window
   * \param [in] alpha New alpha (congestion estimate) value
   */
  typedef void (* CongestionEstimateTracedCallback)(uint32_t bytesAcked, uint32_t bytesMarked, double alpha);

  // Documented in base class
  virtual uint32_t GetSsThresh (Ptr<const TcpSocketState> tcb,
                                uint32_t bytesInFlight);
  virtual Ptr<TcpCongestionOps> Fork ();
  virtual void PktsAcked (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked,
                          const Time &rtt);
  virtual void CwndEvent (Ptr<TcpSocketState> tcb,
                          const TcpSocketState::TcpCAEvent_t event);
private:
  /**
   * \brief Changes state of m_ceState to true
   *
   * \param tcb internal congestion state
   */
  void CeState0to1 (Ptr<TcpSocketState> tcb);

  /**
   * \brief Changes state of m_ceState to false
   *
   * \param tcb internal congestion state
   */
  void CeState1to0 (Ptr<TcpSocketState> tcb);

  /**
   * \brief Updates the value of m_delayedAckReserved
   *
   * \param tcb internal congestion state
   * \param event the congestion window event
   */
  void UpdateAckReserved (Ptr<TcpSocketState> tcb,
                          const TcpSocketState::TcpCAEvent_t event);

  /**
   * \brief Resets the value of m_ackedBytesEcn, m_ackedBytesTotal and m_nextSeq
   *
   * \param tcb internal congestion state
   */
  void Reset (Ptr<TcpSocketState> tcb);

  /**
   * \brief Initialize the value of m_alpha
   *
   * \param alpha DCTCP alpha parameter
   */
  void InitializeDctcpAlpha (double alpha);

  uint32_t m_ackedBytesEcn;             //!< Number of acked bytes which are marked
  uint32_t m_ackedBytesTotal;           //!< Total number of acked bytes
  SequenceNumber32 m_priorRcvNxt;       //!< Sequence number of the first missing byte in data
  bool m_priorRcvNxtFlag;               //!< Variable used in setting the value of m_priorRcvNxt for first time
  double m_alpha;                       //!< Parameter used to estimate the amount of network congestion
  SequenceNumber32 m_nextSeq;           //!< TCP sequence number threshold for beginning a new observation window
  bool m_nextSeqFlag;                   //!< Variable used in setting the value of m_nextSeq for first time
  bool m_ceState;                       //!< DCTCP Congestion Experienced state
  bool m_delayedAckReserved;            //!< Delayed Ack state
  double m_g;                           //!< Estimation gain
  bool m_useEct0;                       //!< Use ECT(0) for ECN codepoint
  bool m_initialized;                   //!< Whether DCTCP has been initialized
  /**
   * \brief Callback pointer for congestion state update
   */
  TracedCallback<uint32_t, uint32_t, double> m_traceCongestionEstimate;
};

} // namespace ns3

#endif /* TCP_DCTCP_MY_H */
