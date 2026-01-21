#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

int main(int argc, char* argv[]){

    NodeContainer nodes;
    nodes.Create(2); //creates 2 nodes

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate",StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay",TimeValue(MilliSeconds(2)));


    NetDeviceContainer devices = csma.Install(nodes); // This is Netdevices. creates network interfaces 1. Attaches one netdevice per node 2. connects them via csma channel

    InternetStackHelper stack;
    stack.Install(nodes); // installs ipv4, udp, tcp and routing into the nodes

    Ipv4AddressHelper address; // this creates an ipv4 addressing scheme
    address.SetBase ("10.1.1.0","255.255.255.0"); //uptil this we donot add this ipv4, we only set the rule

    Ipv4InterfaceContainer interfaces = address.Assign (devices); //assigning ip address to devices

    //after this, the dive is created, ip adresss is assigned to them , they can send packets but there is no application to receive them
    
    //creating a udp server
    UdpServerHelper server(4000); //listens on port 4000
    /*
        udp is connectionless and port based
        This tells the ns3 on which port to listen and which node listens
    */

    //installing the server on node 1

    ApplicationContainer serverApps = server.Install(nodes.Get(1));

    //install defines where it runs, without install it doesn't get executed and only gets created

    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    /*
        after this line the server starts and runs for 10 seconds
        ns3 isn't real time, events only occur when they are scheduled to happen
    */

    //Creating UDP client
    UdpClientHelper client (interfaces.GetAddress(1),4000); //Gets the address of 1->Node 1(Destination node) 2-> port 4000

    client.SetAttribute("MaxPackets",UintegerValue(1000));//Total number of packets that are being sent is 1000
    client.SetAttribute("Interval", TimeValue(MilliSeconds(10))); //sends one packet every 10ms
    client.SetAttribute("PacketSize",UintegerValue(512)); //each packet size is 512 bytes

    /*
        This defines throughput, delay and congestion behavior
        This is my workload model
    */

    ApplicationContainer clientApps = client.Install(nodes.Get(0));

    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    LogComponentEnable("UdpClient",LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer",LOG_LEVEL_INFO);

    csma.EnablePcapAll("my_traces");
    /*
        Pcap are verifiable and tool-independent
    */

    Simulator::Run();
    Simulator::Destroy();

    
    return 0;
}