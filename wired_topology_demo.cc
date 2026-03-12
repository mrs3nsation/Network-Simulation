#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

#include <fstream>

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t nLeftClients = 3;
    uint32_t nRightClients = 3;

    CommandLine cmd;
    cmd.AddValue("nLeftClients", "Number of left-side clients", nLeftClients);
    cmd.AddValue("nRightClients", "Number of right-side clients", nRightClients);
    cmd.Parse(argc, argv);

    // CSV OUTPUT FILE
    std::ofstream csvFile;
    csvFile.open("topology_output.csv");
    csvFile << "FlowId,SrcIP,DstIP,ThroughputMbps,TxPackets,RxPackets\n";

    // NODE CREATION
    NodeContainer leftClients, rightClients;
    leftClients.Create(nLeftClients);
    rightClients.Create(nRightClients);

    NodeContainer accessRouterL, accessRouterR;
    accessRouterL.Create(1);
    accessRouterR.Create(1);

    NodeContainer distRouterL, distRouterR;
    distRouterL.Create(1);
    distRouterR.Create(1);

    NodeContainer server;
    server.Create(1);

    // INTERNET STACK
    InternetStackHelper stack;
    stack.Install(leftClients);
    stack.Install(rightClients);
    stack.Install(accessRouterL);
    stack.Install(accessRouterR);
    stack.Install(distRouterL);
    stack.Install(distRouterR);
    stack.Install(server);

    for (auto router : {accessRouterL.Get(0), accessRouterR.Get(0),distRouterL.Get(0), distRouterR.Get(0)})
    {
        router->GetObject<Ipv4>()->SetAttribute("IpForward", BooleanValue(true));
    }

    // CSMA LINKS
    CsmaHelper csmaFast;
    csmaFast.SetChannelAttribute("DataRate", StringValue("1Gbps"));
    csmaFast.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));

    CsmaHelper csmaCore;
    csmaCore.SetChannelAttribute("DataRate", StringValue("10Mbps"));
    csmaCore.SetChannelAttribute("Delay", TimeValue(MilliSeconds(20)));

    NodeContainer leftLan;
    leftLan.Add(leftClients);
    leftLan.Add(accessRouterL);
    NetDeviceContainer devLeftLan = csmaFast.Install(leftLan);

    NodeContainer rightLan;
    rightLan.Add(rightClients);
    rightLan.Add(accessRouterR);
    NetDeviceContainer devRightLan = csmaFast.Install(rightLan);

    NetDeviceContainer devAccessLtoDistL =
        csmaFast.Install(NodeContainer(accessRouterL.Get(0), distRouterL.Get(0)));

    NetDeviceContainer devAccessRtoDistR =
        csmaFast.Install(NodeContainer(accessRouterR.Get(0), distRouterR.Get(0)));

    NetDeviceContainer devDistLtoServer =
        csmaCore.Install(NodeContainer(distRouterL.Get(0), server.Get(0)));

    NetDeviceContainer devDistRtoServer =
        csmaCore.Install(NodeContainer(distRouterR.Get(0), server.Get(0)));

    // IP ADDRESSING
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    address.Assign(devLeftLan);

    address.SetBase("10.1.2.0", "255.255.255.0");
    address.Assign(devRightLan);

    address.SetBase("10.1.3.0", "255.255.255.0");
    address.Assign(devAccessLtoDistL);

    address.SetBase("10.1.4.0", "255.255.255.0");
    address.Assign(devAccessRtoDistR);

    address.SetBase("10.1.5.0", "255.255.255.0");
    address.Assign(devDistLtoServer);

    address.SetBase("10.1.6.0", "255.255.255.0");
    address.Assign(devDistRtoServer);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // TCP SERVER
    uint16_t port = 50000;

    PacketSinkHelper sinkHelper(
        "ns3::TcpSocketFactory",
        InetSocketAddress(Ipv4Address::GetAny(), port));

    ApplicationContainer sinkApp = sinkHelper.Install(server.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(40.0));

    Ipv4Address serverAddress =
        server.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();

    // TCP clients
    for (uint32_t i = 0; i < nLeftClients; ++i)
    {
        BulkSendHelper sender(
            "ns3::TcpSocketFactory",
            InetSocketAddress(serverAddress, port));
        sender.SetAttribute("MaxBytes", UintegerValue(0));
        ApplicationContainer app = sender.Install(leftClients.Get(i));
        app.Start(Seconds(1.0 + i));
        app.Stop(Seconds(40.0));
    }

    for (uint32_t i = 0; i < nRightClients; ++i)
    {
        BulkSendHelper sender(
            "ns3::TcpSocketFactory",
            InetSocketAddress(serverAddress, port));

        sender.SetAttribute("MaxBytes", UintegerValue(0));
        ApplicationContainer app = sender.Install(rightClients.Get(i));
        app.Start(Seconds(2.0 + i));
        app.Stop(Seconds(40.0));
    }

    // FLOW MONITOR
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> monitor = flowHelper.InstallAll();

    Simulator::Stop(Seconds(40.1));
    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());

    auto stats = monitor->GetFlowStats();

    for (auto const &flow : stats)
    {
        auto t = classifier->FindFlow(flow.first);

        double duration =
            flow.second.timeLastRxPacket.GetSeconds()
            - flow.second.timeFirstTxPacket.GetSeconds();

        double throughput = 0.0;
        if (duration > 0)
        {
            throughput =
                (flow.second.rxBytes * 8.0) / duration / 1e6;
        }

        csvFile
            << flow.first << ","
            << t.sourceAddress << ","
            << t.destinationAddress << ","
            << throughput << ","
            << flow.second.txPackets << ","
            << flow.second.rxPackets << "\n";
    }

    csvFile.close();
    Simulator::Destroy();

    return 0;
}
