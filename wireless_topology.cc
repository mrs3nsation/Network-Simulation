#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>

using namespace ns3;

int main(int argc, char *argv[]){

    //left side client initialization
    NodeContainer leftClientNodes;
    leftClientNodes.Create(5);

    //Right side client initialization
    NodeContainer rightClientNodes;
    rightClientNodes.Create(5);

    //Access Router initialization
    NodeContainer leftAccessRouter;
    leftAccessRouter.Create(1);

    NodeContainer rightAccessRouter;
    rightAccessRouter.Create(1);

    //Distribution Router initialization
    NodeContainer leftDistributionRouter;
    leftDistributionRouter.Create(1);

    NodeContainer rightDistributionRouter;
    rightDistributionRouter.Create(1);

    //Server initialization
    NodeContainer serverNode;
    serverNode.Create(1);

    InternetStackHelper internet;
    internet.Install(leftClientNodes);
    internet.Install(rightClientNodes);
    internet.Install(leftAccessRouter);
    internet.Install(rightAccessRouter);
    internet.Install(leftDistributionRouter);
    internet.Install(rightDistributionRouter);
    internet.Install(serverNode);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);
    wifi.SetRemoteStationManager(
        "ns3::MinstrelHtWifiManager"
    );

    YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default();

    YansWifiPhyHelper phy1;
    phy1.SetChannel(channel1.Create());

    YansWifiChannelHelper channel2 = YansWifiChannelHelper::Default();

    YansWifiPhyHelper phy2;
    phy2.SetChannel(channel2.Create());

    Ssid ssid1 = Ssid("left-lan");

    WifiMacHelper mac1;
    mac1.SetType("ns3::StaWifiMac",
            "Ssid",SsidValue(ssid1),
            "ActiveProbing",BooleanValue(false));

    NetDeviceContainer leftSTADevices;
    leftSTADevices = wifi.Install(phy1, mac1, leftClientNodes);

    mac1.SetType("ns3::ApWifiMac",
                "Ssid",SsidValue(ssid1));

    NetDeviceContainer leftAPDevice;
    leftAPDevice = wifi.Install(phy1,mac1,leftAccessRouter);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> leftPositionAlloc = CreateObject<ListPositionAllocator>();
    leftPositionAlloc->Add(Vector(0.0,0.0,0.0));
    leftPositionAlloc->Add(Vector(5.0,0.0,0.0));
    leftPositionAlloc->Add(Vector(5.0,5.0,0.0));
    leftPositionAlloc->Add(Vector(-5.0,10.0,15.0));
    leftPositionAlloc->Add(Vector(5.0,-10.0,10.0));
    leftPositionAlloc->Add(Vector(-5.0,10.0,-15.0));

    mobility.SetPositionAllocator(leftPositionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    
    mobility.Install(leftClientNodes);
    mobility.Install(leftAccessRouter);

    Ssid ssid2 = Ssid("Right-lan");

    WifiMacHelper mac2;
    mac2.SetType("ns3::StaWifiMac",
            "Ssid",SsidValue(ssid2),
            "ActiveProbing",BooleanValue(false));

    NetDeviceContainer RightStaDevices;
    RightStaDevices = wifi.Install(phy2,mac2,rightClientNodes);

    mac2.SetType("ns3::ApWifiMac",
            "Ssid",SsidValue(ssid2));

    NetDeviceContainer RightAPDevice;
    RightAPDevice = wifi.Install(phy2,mac2,rightAccessRouter);

    mobility.Install(rightClientNodes);
    mobility.Install(rightAccessRouter);

    uint32_t experimentNumber = 8;

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate",StringValue("100Mbps"));
    p2p.SetChannelAttribute("Delay",StringValue("20ms"));

    NetDeviceContainer leftDistToServerDevices;
    leftDistToServerDevices = p2p.Install(leftDistributionRouter.Get(0),serverNode.Get(0));

    NetDeviceContainer rightDistToServerDevices;
    rightDistToServerDevices = p2p.Install(rightDistributionRouter.Get(0),serverNode.Get(0));

    p2p.SetDeviceAttribute("DataRate",StringValue("1000Mbps"));
    p2p.SetChannelAttribute("Delay",StringValue("2ms"));

    NetDeviceContainer leftAccToDistRouterDevices;
    leftAccToDistRouterDevices = p2p.Install(leftAccessRouter.Get(0),leftDistributionRouter.Get(0));

    NetDeviceContainer rightAccToDistRouterDevices;
    rightAccToDistRouterDevices = p2p.Install(rightAccessRouter.Get(0),rightDistributionRouter.Get(0));

    Ipv4AddressHelper address;

    address.SetBase("10.1.5.0","255.255.255.0");
    Ipv4InterfaceContainer leftDistToServerInterfaces;
    leftDistToServerInterfaces = address.Assign(leftDistToServerDevices);

    address.SetBase("10.1.6.0","255.255.255.0");
    Ipv4InterfaceContainer rightDistToServerInterfaces;
    rightDistToServerInterfaces = address.Assign(rightDistToServerDevices);

    address.SetBase("10.1.3.0","255.255.255.0");
    Ipv4InterfaceContainer leftAcctoDistInterfaces;
    leftAcctoDistInterfaces = address.Assign(leftAccToDistRouterDevices);

    address.SetBase("10.1.4.0","255.255.255.0");
    Ipv4InterfaceContainer rightAccToDistInterfaces;
    rightAccToDistInterfaces = address.Assign(rightAccToDistRouterDevices);

    address.SetBase("10.1.1.0","255.255.255.0");

    Ipv4InterfaceContainer leftClientInterface;
    leftClientInterface = address.Assign(leftSTADevices);

    Ipv4InterfaceContainer leftAPInterface;
    leftAPInterface = address.Assign(leftAPDevice);

    address.SetBase("10.1.2.0","255.255.255.0");
    Ipv4InterfaceContainer rightClientInterface;
    rightClientInterface = address.Assign(RightStaDevices);

    Ipv4InterfaceContainer rightAPInterface;
    rightAPInterface = address.Assign(RightAPDevice);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Ipv4GlobalRoutingHelper g;
    // Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>(&std::cout);
    // g.PrintRoutingTableAllAt(Seconds(1),routingStream);

    uint16_t port = 5000;

    Address serverAddress (InetSocketAddress(Ipv4Address::GetAny(),port));
    PacketSinkHelper packetsinkerhelper ("ns3::TcpSocketFactory",serverAddress);

    ApplicationContainer serverapp = packetsinkerhelper.Install(serverNode.Get(0));
    serverapp.Start(Seconds(0.0));
    serverapp.Stop(Seconds(20.1));

    BulkSendHelper bulksender("ns3::TcpSocketFactory",
        InetSocketAddress(Ipv4Address("10.1.5.2"),port));

    bulksender.SetAttribute("MaxBytes",UintegerValue(0));
    bulksender.SetAttribute("SendSize",UintegerValue(1448));

    ApplicationContainer clientapps;

    for(uint32_t i=0;i<leftClientNodes.GetN();i++){
        clientapps.Add(bulksender.Install(leftClientNodes.Get(i)));
    }

    for(uint32_t i=0;i< rightClientNodes.GetN();i++){
        clientapps.Add(bulksender.Install(rightClientNodes.Get(i)));
    }

    clientapps.Start(Seconds(2.0));
    clientapps.Stop(Seconds(20.0));

    FlowMonitorHelper flowmonitor;
    Ptr<FlowMonitor> monitor = flowmonitor.InstallAll();

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonitor.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    std::fstream checkFile("Wireless_transmission.csv");
    bool fileEmpty = checkFile.peek() == std::ifstream::traits_type::eof();
    checkFile.close();

    std::ofstream csvFile;
    csvFile.open("Wireless_transmission.csv", std::ios::app);

    if(fileEmpty){
        csvFile << "Experiment_ID,FlowID,Source,Destination,TxPackets,RxPackets,PacketLossRate,"
        << "AverageDelay,AverageJitter,FlowDuration,ThroughputMbps,LinkUtilization\n";
    }

    double sumThroughput = 0.0;
    double sumThroughputsq = 0.0;
    int flowcount = 0;

    for (auto const &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        double txPackets = flow.second.txPackets;
        double rxPackets = flow.second.rxPackets;

        double packetLossRate = (txPackets - rxPackets)/txPackets;
        double avgDelay = 0.0;
        if(rxPackets>0){
            avgDelay = flow.second.delaySum.GetSeconds() / flow.second.rxPackets;
        }

        double avgJitter = 0.0;
        if(rxPackets>0){
            avgJitter = flow.second.jitterSum.GetSeconds() / rxPackets;
        }

        double flowDuration = (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds());

        double ThroughputMbps = flow.second.rxPackets * 8.0 / 20.0 / 1000000;
        sumThroughput += ThroughputMbps;
        sumThroughputsq += ThroughputMbps * ThroughputMbps;
        flowcount++;
        double LinkUtilization = (flow.second.rxPackets * 8.0) / (10000000 * flowDuration);

        csvFile << experimentNumber << ","
            << flow.first << ","
            << t.sourceAddress << ","
            << t.destinationAddress << ","
            << txPackets << ","
            << rxPackets << ","
            << packetLossRate << ","
            << avgDelay << ","
            << avgJitter << ","
            << flowDuration << ","
            << ThroughputMbps << ","
            << LinkUtilization << "\n";
    }

    double fairnessIndex = (sumThroughput * sumThroughput) / (flowcount * sumThroughputsq);
    std::cout <<"fairness index \n" << fairnessIndex << "\n" << std::ends;

    csvFile.close();

    Simulator::Destroy();
    return 0;

}