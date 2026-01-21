#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>
#include <iostream>

using namespace ns3;

int main(int argc, char *argv[]){

    std::cout << "Program started";

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate",StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay",TimeValue(MilliSeconds(2)));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0","255.255.255.0");

    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    FlowMonitorHelper flowhelper;
    Ptr<FlowMonitor> flowmonitor = flowhelper.InstallAll(); 

    UdpServerHelper server(4000);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));


    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper client(interfaces.GetAddress(1),4000);
    client.SetAttribute("MaxPackets",UintegerValue(1000));
    client.SetAttribute("PacketSize",UintegerValue(512));
    client.SetAttribute("Interval",TimeValue(MilliSeconds(10)));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));

    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.1));    
    Simulator::Run();

    flowmonitor->CheckForLostPackets();

    Ptr<FlowClassifier> baseClassifier = flowhelper.GetClassifier();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(baseClassifier);

    if(classifier == nullptr){
        std::cerr << "Error, classifies is not a ipv4classifiers";
        return 1;
    }

    std::map<FlowId,FlowMonitor::FlowStats> stats = flowmonitor->GetFlowStats();

    std::ofstream csvFile("metrics.csv");
    csvFile << "flow_id,src_ip,dst_ip,tx_packets,rx_packets,lost_packets,throughput_mbps,average_delay,pdr,average_jitter\n";

    for(const auto &flow : stats){
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        uint32_t txPackets = flow.second.txPackets;
        uint32_t rxPackets = flow.second.rxPackets;
        uint32_t lostpackets = txPackets - rxPackets;

        double duration = flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds();

        double throughputMbps = 0.0;
        if(duration > 0){
            throughputMbps = (flow.second.rxBytes * 8.0) / duration / 1e6;
        }

        double avgDelay = 0.0;
        if(rxPackets> 0){ //flow.second.delaySum is already present inside the flow monitor module
            avgDelay = flow.second.delaySum.GetSeconds() / rxPackets;
        }

        double pdr = 0.0;
        if(txPackets > 0){
            pdr = static_cast<double>(rxPackets) / txPackets;
        }

        double avgJitter = 0.0;
        if(flow.second.rxPackets > 1){ //flow.second.jitterSum is already present inside the flow monitor module
            avgJitter = flow.second.jitterSum.GetSeconds() / (flow.second.rxPackets - 1);
        }

        csvFile
            <<flow.first << ","
            << t.sourceAddress << ","
            << t.destinationAddress << ","
            << txPackets << ","
            << rxPackets << ","
            << lostpackets << ","
            << throughputMbps << ","
            << avgDelay <<","
            << pdr << ","
            << avgJitter
            << "\n";
        
    }
    std::cout <<"\nprogram finished";
    Simulator::Destroy();

    return 0;
}