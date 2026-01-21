#include <ns3/core-module.h>
#include <ns3/applications-module.h>
#include <ns3/network-module.h>
#include <ns3/internet-module.h>
#include <ns3/csma-module.h>
#include <ns3/flow-monitor-module.h>
#include <ns3/traffic-control-module.h>

#include <fstream>
#include <iostream>

using namespace ns3;

std::ofstream csvFile;
std::ofstream cwndFile;

void cwndChange(uint32_t oldcwnd, uint32_t newcwnd){
    cwndFile
        << Simulator::Now().GetSeconds() <<","
        << newcwnd << "\n";
}

void
TraceCwnd(Ptr<Application> app)
{
    Ptr<BulkSendApplication> bulk =
        DynamicCast<BulkSendApplication>(app);
    if (!bulk)
    {
        return;
    }

    Ptr<Socket> sock = bulk->GetSocket();
    if (!sock)
    {
        Simulator::Schedule(
            MilliSeconds(1),
            &TraceCwnd,
            app
        );
        return;
    }

    Ptr<TcpSocketBase> tcpSock =
        DynamicCast<TcpSocketBase>(sock);
    if (!tcpSock)
    {
        return;
    }

    tcpSock->TraceConnectWithoutContext(
        "CongestionWindow",
        MakeCallback(&cwndChange)
    );
}

int main(int argc, char *argv[]){

    cwndFile.open("cwnd_trace.csv");
    cwndFile << "Time,CwndBytes\n" ;
    csvFile.open("cwnd_tracing.csv");
    csvFile << "flow_id,src_ip,dst_ip,lost_packets,throughput_mbps,avg_delay,pdr,avg_jitter\n";


    Config::SetDefault(
        "ns3::TcpL4Protocol::SocketType",
        TypeIdValue(TcpNewReno::GetTypeId())
    );

    NodeContainer client;
    NodeContainer router;
    NodeContainer server;

    client.Create(1);
    router.Create(1);
    server.Create(1);

    InternetStackHelper stack;
    stack.Install(client);
    stack.Install(router);
    stack.Install(server);

    Ptr<Ipv4> ipv4 = router.Get(0)->GetObject<Ipv4>();
    ipv4->SetAttribute("IpForward",BooleanValue(true));

    CsmaHelper csmaFast;
    csmaFast.SetChannelAttribute("DataRate",StringValue("1Gbps"));
    csmaFast.SetChannelAttribute("Delay",TimeValue(MilliSeconds(1)));

    CsmaHelper csmaSlow;
    csmaSlow.SetChannelAttribute("DataRate",StringValue("10Mbps"));
    csmaSlow.SetChannelAttribute("Delay",TimeValue(MilliSeconds(20)));

    NetDeviceContainer devClientRouter = csmaFast.Install(NodeContainer(client.Get(0),router.Get(0)));

    NetDeviceContainer devRouterServer = csmaSlow.Install(NodeContainer(router.Get(0),server.Get(0)));

    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0","255.255.255.0");
    Ipv4InterfaceContainer ifClientRouter = address.Assign(devClientRouter);

    address.SetBase("10.1.2.0","255.255.255.0");
    Ipv4InterfaceContainer ifRouterServer = address.Assign(devRouterServer);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    FlowMonitorHelper flowhelper;
    Ptr<FlowMonitor> flowmonitor = flowhelper.InstallAll();

    TrafficControlHelper tch;
    tch.Uninstall(devRouterServer.Get(0));
    tch.SetRootQueueDisc("ns3::PfifoFastQueueDisc");
    tch.Install(devRouterServer.Get(0));

    uint16_t port = 32000;
    PacketSinkHelper sinkhelper(
        "ns3::TcpSocketFactory",
        InetSocketAddress(Ipv4Address::GetAny(),port)
    );

    ApplicationContainer sinkApp = sinkhelper.Install(server.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(20.0));

    BulkSendHelper bulksender(
        "ns3::TcpSocketFactory",
        InetSocketAddress(ifRouterServer.GetAddress(1),port)
    );

    bulksender.SetAttribute("MaxBytes",UintegerValue(0));

    ApplicationContainer clientApps = bulksender.Install(client.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(20.0));

    Simulator::Schedule(
    Seconds(1.0),
    &TraceCwnd,
    clientApps.Get(0)
    );  

    
    Simulator::Stop(Seconds(20.1));
    Simulator::Run();

    Ptr<FlowClassifier> baseClassifier = flowhelper.GetClassifier();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(baseClassifier);

    std::map<FlowId, FlowMonitor::FlowStats> stats = flowmonitor->GetFlowStats();

    for(const auto &flow : stats){

        FlowId id = flow.first;
        FlowMonitor::FlowStats st = flow.second;

        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(id);

        uint32_t txPackets = st.txPackets;
        uint32_t rxPackets = st.rxPackets;
        uint32_t lost_packets = txPackets - rxPackets;

        double duration = st.timeLastRxPacket.GetSeconds() - st.timeFirstTxPacket.GetSeconds();

        double throughputMbps = 0.0;
        if(duration>0){
            throughputMbps = (st.rxBytes * 8.0) / duration / 1e6;
        }

        double avgDelay = 0.0;
        if(rxPackets > 0){
            avgDelay = (st.delaySum.GetSeconds()/rxPackets);
        }

        double pdr = 0.0;
        if(txPackets>0){
            pdr = static_cast<double>(rxPackets) / txPackets;
        }

        double avgJitter = 0.0;
        if(rxPackets > 1){
            avgJitter = (st.jitterSum.GetSeconds())/(rxPackets - 1);
        }

        csvFile
            << id << ","
            << t.sourceAddress << ","
            << t.destinationAddress << ","
            << lost_packets << ","
            << throughputMbps << ","
            << avgDelay << ","
            << pdr << ","
            << avgJitter << "\n";
    }

    Simulator::Destroy();
    cwndFile.close();
    csvFile.close();
    return 0;

}