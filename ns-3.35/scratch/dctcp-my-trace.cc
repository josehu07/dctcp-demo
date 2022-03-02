/*
 * Guanzhou Hu @ UW-Madison, CS740, Spring 2022
 *
 * This file contains a simulation experiment on a minimal network topology
 * and traces how the congestion window size evolves in DCTCP vs. Reno.
 */

#include <iostream>
#include <iomanip>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"


using namespace ns3;


// Queue length at switch T.
static uint32_t
GetQueueLen (Ptr<QueueDisc> queue)
{
  return queue->GetNPackets ();
}

// Congestion window size at sender S.
// static void
// GetCwndSize ()
// {

// }

void
PrintProgress (Time interval, Ptr<QueueDisc> queue)
{
  uint32_t queueLen = GetQueueLen(queue);
  std::cout << std::fixed << std::setprecision (1)
            << Simulator::Now ().GetSeconds () << " "
            << queueLen << std::endl;
  Simulator::Schedule (interval, &PrintProgress, interval, queue);
}


int
main (int argc, char *argv[])
{
  // experiment configurations
  std::string tcpTypeId = "TcpDctcpMy";
  Time flowStartupWindow = Seconds (1);
  Time convergenceTime = Seconds (3);
  Time measurementWindow = Seconds (1);
  bool enableSwitchEcn = true;
  std::string clientDataRate = "10Gbps";
  Time progressInterval = MilliSeconds (100);

  CommandLine cmd (__FILE__);
  cmd.AddValue ("tcpTypeId", "ns-3 TCP TypeId", tcpTypeId);
  cmd.AddValue ("flowStartupWindow", "startup time window", flowStartupWindow);
  cmd.AddValue ("convergenceTime", "convergence time", convergenceTime);
  cmd.AddValue ("measurementWindow", "measurement window", measurementWindow);
  cmd.AddValue ("enableSwitchEcn", "enable ECN at switches", enableSwitchEcn);
  cmd.AddValue ("clientDataRate", "client app sending rate", clientDataRate);
  cmd.Parse (argc, argv);

  Time startTime = Seconds (0);
  Time stopTime = flowStartupWindow + convergenceTime + measurementWindow;

  // general TCP configurations
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::" + tcpTypeId));
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1448));
  Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (2));
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (false));

  // RED queue configurations
  Config::SetDefault ("ns3::RedQueueDisc::UseEcn", BooleanValue (enableSwitchEcn));
  Config::SetDefault ("ns3::RedQueueDisc::UseHardDrop", BooleanValue (false));
  Config::SetDefault ("ns3::RedQueueDisc::MeanPktSize", UintegerValue (1500));
  Config::SetDefault ("ns3::RedQueueDisc::MaxSize", QueueSizeValue (QueueSize ("2666p")));
  Config::SetDefault ("ns3::RedQueueDisc::QW", DoubleValue (1));
  Config::SetDefault ("ns3::RedQueueDisc::MinTh", DoubleValue (20));
  Config::SetDefault ("ns3::RedQueueDisc::MaxTh", DoubleValue (60));  // TODO?

  // two hosts and one switch T
  Ptr<Node> nodeS = CreateObject<Node> ();
  Ptr<Node> nodeR = CreateObject<Node> ();
  Ptr<Node> nodeT = CreateObject<Node> ();

  // network link type
  PointToPointHelper link;
  link.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
  link.SetChannelAttribute ("Delay", StringValue ("10us"));

  // connect S to T and T to R
  NetDeviceContainer devST = link.Install (nodeS, nodeT);
  NetDeviceContainer devTR = link.Install (nodeT, nodeR);

  // internet stack on nodes
  InternetStackHelper stack;
  stack.InstallAll ();

  // set RED traffic control
  TrafficControlHelper red;
  red.SetRootQueueDisc ("ns3::RedQueueDisc",
                        "LinkBandwidth", StringValue ("1Gbps"),
                        "LinkDelay", StringValue ("10us"),
                        "MinTh", DoubleValue (20),
                        "MaxTh", DoubleValue (60));
  QueueDiscContainer queueST = red.Install (devST);
  QueueDiscContainer queueTR = red.Install (devTR);

  // associate IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer intfST = address.Assign (devST);
  Ipv4InterfaceContainer intfTR = address.Assign (devTR);
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // sink application on receiver R
  uint16_t port = 50001;
  Address sinkLocalAddr (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddr);
  ApplicationContainer sinkApp = sinkHelper.Install (nodeR);
  Ptr<PacketSink> sink = sinkApp.Get (0)->GetObject<PacketSink> ();
  sinkApp.Start (startTime);
  sinkApp.Stop (stopTime);

  // on-off application on sender S
  OnOffHelper onoffHelper ("ns3::TcpSocketFactory", Address ());
  onoffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoffHelper.SetAttribute ("DataRate", DataRateValue (DataRate (clientDataRate)));
  onoffHelper.SetAttribute ("PacketSize", UintegerValue (1000));
  ApplicationContainer onoffApp;
  AddressValue remoteAddr (InetSocketAddress (intfTR.GetAddress (1), port));
  onoffHelper.SetAttribute ("Remote", remoteAddr);
  onoffApp.Add (onoffHelper.Install (nodeS));
  onoffApp.Start (startTime);
  onoffApp.Stop (stopTime);

  // simulation
  Simulator::Schedule (progressInterval, &PrintProgress, progressInterval, queueTR.Get (0));
  Simulator::Stop (stopTime + TimeStep (1));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
