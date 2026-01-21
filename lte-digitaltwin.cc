#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable ("LteHelper", LOG_LEVEL_INFO);

  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);
  

  NodeContainer ueNodes;
  NodeContainer enbNodes;
  ueNodes.Create (1);
  enbNodes.Create (1);
  InternetStackHelper internet;
  internet.Install(ueNodes);

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (ueNodes);
  mobility.Install (enbNodes);
  
  

  NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueDevs  = lteHelper->InstallUeDevice (ueNodes);

  lteHelper->Attach (ueDevs.Get (0), enbDevs.Get (0));
  //asigning ip addresses
  Ipv4InterfaceContainer ueIpIfaces;
  ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNodes.Get(0)->GetObject<Ipv4>());
  ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(),1);
  
  // Create remote host (represents the Internet)
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create(1);
  Ptr<Node> remoteHost = remoteHostContainer.Get(0);

  // Install Internet stack on remote host
  internet.Install(remoteHostContainer);

  // Create point-to-point link between PGW and remote host
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
  p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.01)));

  Ptr<Node> pgw = epcHelper->GetPgwNode();
  NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

  // Assign IP addresses to the backhaul link
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIfaces = ipv4h.Assign(internetDevices);

  // Configure routing on the remote host
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
    ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());

  remoteHostStaticRouting->AddNetworkRouteTo(
    Ipv4Address("7.0.0.0"),
    Ipv4Mask("255.0.0.0"),
    1
  );

  uint16_t dlPort = 1234;

  // UDP server on remote host
  UdpServerHelper udpServer(dlPort);
  ApplicationContainer serverApps = udpServer.Install(remoteHost);
  serverApps.Start(Seconds(0.1));
  serverApps.Stop(Seconds(1.0));

  // UDP client on UE
  UdpClientHelper udpClient(internetIfaces.GetAddress(1), dlPort);
  udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
  udpClient.SetAttribute("PacketSize", UintegerValue(512));
  udpClient.SetAttribute("MaxPackets", UintegerValue(1000000));

  ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(0));
  clientApps.Start(Seconds(0.2));
  clientApps.Stop(Seconds(1.0));

  // Enable FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();


    Simulator::Stop (Seconds (1.0));
    Simulator::Run ();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier =
    DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());

  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  for (auto const& flow : stats)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);

    std::cout << "Flow ID: " << flow.first << "\n";
    std::cout << "  Src: " << t.sourceAddress << "\n";
    std::cout << "  Dst: " << t.destinationAddress << "\n";
    std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
    std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";

    if (flow.second.rxPackets > 0)
    {
      std::cout << "  Throughput: "
                << (flow.second.rxBytes * 8.0 /
                    (flow.second.timeLastRxPacket.GetSeconds() -
                    flow.second.timeFirstTxPacket.GetSeconds()) /
                    1e6)
                << " Mbps\n";

      std::cout << "  Mean Delay: "
                << (flow.second.delaySum.GetSeconds() /
                    flow.second.rxPackets)
                << " s\n";
    }
  }


  Simulator::Destroy ();

  return 0;

}
