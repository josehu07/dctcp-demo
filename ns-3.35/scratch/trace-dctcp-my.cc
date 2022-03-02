/*
 * Guanzhou Hu @ UW-Madison, CS740, Spring 2022
 *
 * This file contains a simulation experiment on a minimal network topology
 * and traces how the congestion window size evolves in DCTCP vs. Reno.
 *
 * The topology contains 20 senders, 1 receiver, and a switch T. Senders each
 * connect to T through a 1Gbps link, and the receiver is connected to T
 * through a 10Gbps link. All senders send a steady stream of data at 1Gbps
 * rate.
 */

#include <iostream>
#include <iomanip>
#include <numeric>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"


using namespace ns3;


// Cwnd in bytes on sender S0.
uint32_t cwndSizeS0 = 0;

// Sinked bytes on receiver app.
std::vector<uint64_t> rxSinkBytes(20, 0);

void
TraceCwndSizeS0 (uint32_t old_cwnd, uint32_t new_cwnd)
{
  cwndSizeS0 = new_cwnd;
}

void
TraceRxSinkBytes (size_t i, Ptr<const Packet> p, const Address& a)
{
  rxSinkBytes[i] += p->GetSize ();
}


void
PrintProgress (Time interval, Ptr<QueueDisc> queue)
{
  uint32_t queueLen = queue->GetNPackets ();
  uint64_t rxTotalBytes = 0;
  for (uint64_t& b : rxSinkBytes)
    rxTotalBytes += b;

  std::cout << std::fixed << std::setprecision (2)
            << std::setw (7) << Simulator::Now ().GetSeconds () << "  "
            << std::setw (11) << queueLen << "  "
            << std::setw (13) << cwndSizeS0 << "  "
            << std::setw (13) << rxTotalBytes << std::endl;
  
  Simulator::Schedule (interval, &PrintProgress, interval, queue);
}


//////////////////////////////
// SimpleSource application //
//////////////////////////////

// This appliation is adopted from `ns3tcp-cwnd-test-suite.cc`. It exposes
// a direct handle to the TCP socket.

class SimpleSource : public Application 
{
public:

  SimpleSource ();
  virtual ~SimpleSource();

  /**
   * Register this type.
   * \return The TypeId.
   */
  static TypeId GetTypeId (void);
  
  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize,
              DataRate dataRate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  DataRate        m_dataRate;
  EventId         m_sendEvent;
  bool            m_running;
  uint32_t        m_packetsSent;
};

SimpleSource::SimpleSource ()
  : m_socket (0), 
    m_peer (), 
    m_packetSize (0), 
    m_dataRate (0), 
    m_sendEvent (), 
    m_running (false), 
    m_packetsSent (0)
{
}

SimpleSource::~SimpleSource()
{
  m_socket = 0;
}

/* static */
TypeId
SimpleSource::GetTypeId (void)
{
  static TypeId tid = TypeId ("SimpleSource")
    .SetParent<Application> ()
    .SetGroupName ("Stats")
    .AddConstructor<SimpleSource> ()
    ;
  return tid;
}
  
void
SimpleSource::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize,
                     DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_dataRate = dataRate;
}

void
SimpleSource::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;
  m_socket->Bind ();
  m_socket->Connect (m_peer);
  SendPacket ();
}

void 
SimpleSource::StopApplication (void)
{
  m_running = false;

  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }

  if (m_socket)
    {
      m_socket->Close ();
    }
}

void 
SimpleSource::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);
  ScheduleTx ();
}

void 
SimpleSource::ScheduleTx (void)
{
  if (m_running)
    {
      Time tNext (Seconds (m_packetSize * 8 /
                           static_cast<double> (m_dataRate.GetBitRate ())));
      m_sendEvent = Simulator::Schedule (tNext, &SimpleSource::SendPacket, this);
    }
}


//////////////////////////
// Simulation procedure //
//////////////////////////

int
main (int argc, char *argv[])
{
  // experiment configurations
  std::string tcpTypeId = "TcpDctcpMy";
  Time flowStartupWindow = Seconds (1);
  Time convergenceTime = Seconds (1);
  Time measurementWindow = Seconds (1);

  CommandLine cmd (__FILE__);
  cmd.AddValue ("tcpTypeId", "ns-3 TCP TypeId", tcpTypeId);
  cmd.AddValue ("flowStartupWindow", "startup time window", flowStartupWindow);
  cmd.AddValue ("convergenceTime", "convergence time", convergenceTime);
  cmd.AddValue ("measurementWindow", "measurement window", measurementWindow);
  cmd.Parse (argc, argv);

  Time startTime = Seconds (0);
  Time stopTime = flowStartupWindow + convergenceTime + measurementWindow;

  // general TCP configurations
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::" + tcpTypeId));
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1448));
  Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (2));
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (false));

  // RED queue configurations
  Config::SetDefault ("ns3::RedQueueDisc::UseEcn", BooleanValue (true));
  Config::SetDefault ("ns3::RedQueueDisc::UseHardDrop", BooleanValue (false));
  Config::SetDefault ("ns3::RedQueueDisc::MeanPktSize", UintegerValue (1500));
  Config::SetDefault ("ns3::RedQueueDisc::MaxSize", QueueSizeValue (QueueSize ("1000p")));
  Config::SetDefault ("ns3::RedQueueDisc::QW", DoubleValue (1));

  // 10 senders, 1 receiver R, and 1 switch T
  NodeContainer senders;
  senders.Create (20);
  Ptr<Node> nodeR = CreateObject<Node> ();
  Ptr<Node> nodeT = CreateObject<Node> ();

  // network link types
  PointToPointHelper linkST;
  linkST.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
  linkST.SetChannelAttribute ("Delay", StringValue ("10us"));
  PointToPointHelper linkTR;
  linkTR.SetDeviceAttribute ("DataRate", StringValue ("10Gbps"));
  linkTR.SetChannelAttribute ("Delay", StringValue ("10us"));

  // connect senders to T and T to R
  std::vector<NetDeviceContainer> devSTs;
  for (size_t i = 0; i < 20; ++i)
    devSTs.push_back (linkST.Install (senders.Get (i), nodeT));
  NetDeviceContainer devTR = linkTR.Install (nodeT, nodeR);

  // internet stack on nodes
  InternetStackHelper stack;
  stack.InstallAll ();

  // set RED traffic control
  TrafficControlHelper red1G;
  red1G.SetRootQueueDisc ("ns3::RedQueueDisc",
                          "LinkBandwidth", StringValue ("1Gbps"),
                          "LinkDelay", StringValue ("10us"),
                          "MinTh", DoubleValue (20),
                          "MaxTh", DoubleValue (60));
  for (size_t i = 0; i < 20; ++i)
    red1G.Install (devSTs[i].Get (1));
  TrafficControlHelper red10G;
  red10G.SetRootQueueDisc ("ns3::RedQueueDisc",
                           "LinkBandwidth", StringValue ("10Gbps"),
                           "LinkDelay", StringValue ("10us"),
                           "MinTh", DoubleValue (50),
                           "MaxTh", DoubleValue (150));
  QueueDiscContainer queues = red10G.Install (devTR.Get (0));

  // associate IP addresses
  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> intfSTs;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  for (size_t i = 0; i < 20; ++i) {
    intfSTs.push_back (address.Assign (devSTs[i]));
    address.NewNetwork ();
  }
  Ipv4InterfaceContainer intfTR = address.Assign (devTR);
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // create dummy applications on hosts
  std::vector<Ptr<PacketSink>> sinks;
  std::vector<Ptr<Socket>> txSockets;
  for (size_t i = 0; i < 20; ++i) {
    // sink application on receiver R
    uint16_t port = 50000 + i;
    Address sinkLocalAddr (InetSocketAddress (Ipv4Address::GetAny (), port));
    PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddr);
    ApplicationContainer sinkApp = sinkHelper.Install (nodeR);
    Ptr<PacketSink> sink = sinkApp.Get (0)->GetObject<PacketSink> ();
    sinks.push_back (sink);
    sinkApp.Start (startTime);
    sinkApp.Stop (stopTime);

    // simple-source application on sender
    Ptr<Socket> socket = Socket::CreateSocket (senders.Get (i), TcpSocketFactory::GetTypeId ());
    txSockets.push_back (socket);
    Ptr<SimpleSource> sourceApp = CreateObject<SimpleSource> ();
    Address sinkRemoteAddr (InetSocketAddress (intfTR.GetAddress (1), port));
    sourceApp->Setup (socket, sinkRemoteAddr, 1000, DataRate ("1Gbps"));
    senders.Get (i)->AddApplication (sourceApp);
    sourceApp->SetStartTime (startTime + i * flowStartupWindow / 20);
    sourceApp->SetStopTime (stopTime);
  }

  // simulation
  Time progressInterval = MilliSeconds (1);
  for (size_t i = 0; i < 20; ++i)
    sinks[i]->TraceConnectWithoutContext ("Rx", MakeBoundCallback (&TraceRxSinkBytes, i));
  txSockets[0]->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&TraceCwndSizeS0));
  printf("%7s  %11s  %13s  %13s\n",
         "Time(s)", "Queue(pkts)", "CwndS0(bytes)", "RxSink(bytes)");
  Simulator::Schedule (progressInterval, &PrintProgress, progressInterval, queues.Get (0));
  Simulator::Stop (stopTime + TimeStep (1));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
