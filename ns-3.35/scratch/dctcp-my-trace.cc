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


std::vector<uint64_t> rxSinkBytes(20, 0);

// Sinked bytes on receiver app.
static void
TraceSink (size_t i, Ptr<const Packet> p, const Address& a)
{
  rxSinkBytes[i] += p->GetSize ();
}


void
PrintProgress (Time interval, Ptr<QueueDisc> queue, Ptr<TcpSocketState> socketS0)
{
  uint32_t queueLen = queue->GetNPackets ();
  uint32_t cwndSize = socketS0->m_cWnd.Get ();
  uint64_t rxTotalBytes = 0;
  for (uint64_t& b : rxSinkBytes)
    rxTotalBytes += b;

  std::cout << std::fixed << std::setprecision (2)
            << std::setw (7) << Simulator::Now ().GetSeconds () << "  "
            << std::setw (11) << queueLen << "  "
            << std::setw (13) << cwndSize << "  "
            << std::setw (13) << rxTotalBytes << std::endl;
  
  Simulator::Schedule (interval, &PrintProgress, interval, queue);
}


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
  Config::SetDefault ("ns3::RedQueueDisc::MaxSize", QueueSizeValue (QueueSize ("2666p")));
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
  Ptr<TcpSocketState> socketS0 = senders.Get (0)->GetObject<TcpSocketState> ();

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

    // on-off application on sender
    OnOffHelper onoffHelper ("ns3::TcpSocketFactory", Address ());
    onoffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    onoffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    onoffHelper.SetAttribute ("DataRate", DataRateValue (DataRate ("1Gbps")));
    onoffHelper.SetAttribute ("PacketSize", UintegerValue (1000));
    ApplicationContainer onoffApp;
    AddressValue remoteAddr (InetSocketAddress (intfTR.GetAddress (1), port));
    onoffHelper.SetAttribute ("Remote", remoteAddr);
    onoffApp.Add (onoffHelper.Install (senders.Get (i)));
    onoffApp.Start (startTime + i * flowStartupWindow / 20);
    onoffApp.Stop (stopTime);
  }

  // simulation
  Time progressInterval = MilliSeconds (10);
  for (size_t i = 0; i < 20; ++i)
    sinks[i]->TraceConnectWithoutContext ("Rx", MakeBoundCallback (&TraceSink, i));
  printf("%7s  %11s %11s  %13s\n", "Time(s)", "Queue(pkts)", "CwndS0(bytes)", "RxSink(bytes)");
  Simulator::Schedule (progressInterval, &PrintProgress, progressInterval, queues.Get (0), socketS0);
  Simulator::Stop (stopTime + TimeStep (1));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
