/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* Author: Antonio Marsico <antonio.marsico@bt.com> */

#include <ns3/simple-channel.h>
#include <ns3/internet-module.h>
#include "ns3/csma-module.h"
#include "ns3/core-module.h"
#include "ns3/node-container.h"
#include "ns3/simple-net-device.h"
#include "ns3/simple-net-device-helper.h"
#include "ns3/log.h"
#include "ns3/node-container.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/ipv4-global-routing.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/bridge-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HULAExample");

int 
main (int argc, char *argv[])
{
    bool verbose = true;

    CommandLine cmd;
    cmd.AddValue ("verbose", "Tell application to log if true", verbose);

    cmd.Parse (argc,argv);

    if (verbose)
    {
        LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
        LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);
        LogComponentEnable ("HULAExample", LOG_LEVEL_INFO);
    }

    NodeContainer m_nodes;
    m_nodes.Create(6);

    //connect node0 to node1
    PointToPointHelper channel1;
    channel1.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    channel1.SetChannelAttribute ("Delay", StringValue ("2ms"));
    NetDeviceContainer net = channel1.Install (m_nodes.Get (0), m_nodes.Get (1));

    NetDeviceContainer bridgeFacingDevices;
    NetDeviceContainer switchDevices;

    //connect node1, node3 and node5 to node2 (switch)
    CsmaHelper channel3;
    channel3.SetChannelAttribute ("DataRate", StringValue ("5Mbps"));
    channel3.SetChannelAttribute ("Delay", StringValue ("2ms"));

    NetDeviceContainer net3 = channel3.Install (NodeContainer(m_nodes.Get (2), m_nodes.Get (1)));
    bridgeFacingDevices.Add (net3.Get (1));
    switchDevices.Add (net3.Get (0));

    net3 = channel3.Install (NodeContainer(m_nodes.Get (2), m_nodes.Get (3)));
    bridgeFacingDevices.Add (net3.Get (1));
    switchDevices.Add (net3.Get (0));

    net3 = channel3.Install (NodeContainer(m_nodes.Get (2), m_nodes.Get (5)));
    bridgeFacingDevices.Add (net3.Get (1));
    switchDevices.Add (net3.Get (0));

    //connect node3 to node4
    Ptr<SimpleChannel> channel4 = CreateObject <SimpleChannel> ();
    SimpleNetDeviceHelper simpleHelper4;
    NetDeviceContainer net4 = simpleHelper4.Install (m_nodes.Get (3), channel4);
    net4.Add (simpleHelper4.Install (m_nodes.Get (4), channel4));

    Ptr<Node> switchNode = m_nodes.Get (2);
    BridgeHelper bridge;
    bridge.Install (switchNode, switchDevices);

    InternetStackHelper internet;
    // By default, InternetStackHelper adds a static and global routing
    // implementation.  We just want the global for this test.
    //Ipv4GlobalRoutingHelper ipv4RoutingHelper;
    //internet.SetRoutingHelper (ipv4RoutingHelper);

    internet.Install (m_nodes.Get (0));
    internet.Install (m_nodes.Get (1));
    // m_nodes.Get (2) is bridge node
    internet.Install (m_nodes.Get (3));
    internet.Install (m_nodes.Get (4));
    internet.Install (m_nodes.Get (5));

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer serverInterface = address.Assign (net);

    address.SetBase ("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer deviceConnectedToBridge = address.Assign (bridgeFacingDevices);

    address.SetBase ("10.1.3.0", "255.255.255.0");
    address.Assign (net4);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    Ipv4ListRoutingHelper list ();

    NS_LOG_INFO ("Entering routing tables.");

    Ptr<Ipv4L3Protocol> ip0 = m_nodes.Get (1)->GetObject<Ipv4L3Protocol> ();
    Ptr<Ipv4RoutingProtocol> routing0 = ip0->GetRoutingProtocol ();
    Ptr<Ipv4GlobalRouting> globalRouting0 = routing0->GetObject <Ipv4GlobalRouting> ();

    Ptr<Ipv4StaticRouting> staticRouting =
            Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (m_nodes.Get (1)->GetObject<Ipv4> ()->GetRoutingProtocol ());

    NS_LOG_INFO("Routing " << staticRouting->GetTypeId());

    for (uint32_t i = 0; i < staticRouting->GetNRoutes (); i++)
    {
        Ipv4RoutingTableEntry route = staticRouting->GetRoute (i);
        NS_LOG_INFO ("entry dest " << route.GetDest ()
        << " gw " << route.GetGateway ()
        << " out " << route.GetInterface()
        << " metric " << staticRouting->GetMetric(i));
    }
    Ptr<Ipv4GlobalRouting> globalRouting =
            Ipv4RoutingHelper::GetRouting <Ipv4GlobalRouting> (m_nodes.Get (1)->GetObject<Ipv4> ()->GetRoutingProtocol ());

    NS_LOG_INFO("Routing " << globalRouting->GetTypeId());

    for (uint32_t i = 0; i < globalRouting->GetNRoutes (); i++)
    {
        Ipv4RoutingTableEntry* route = globalRouting->GetRoute (i);
        NS_LOG_INFO ("entry dest " << route->GetDest ()
                                   << " gw " << route->GetGateway ()
                                   << " out " << route->GetInterface());
    }


    NS_LOG_INFO ("Create Applications.");
    uint16_t port = 9;   // Discard port (RFC 863)

    OnOffHelper onoff ("ns3::TcpSocketFactory",
                       Address (InetSocketAddress (deviceConnectedToBridge.GetAddress(2), port)));
    //onoff.SetConstantRate (DataRate ("5Mb/s"));

    ApplicationContainer app = onoff.Install (m_nodes.Get (0));
    // Start the application
    app.Start (Seconds (1.0));
    app.Stop (Seconds (10.0));

    // Create an optional packet sink to receive these packets
    PacketSinkHelper sink ("ns3::TcpSocketFactory",
                           Address (InetSocketAddress (Ipv4Address::GetAny (), port)));
    app = sink.Install (m_nodes.Get (5));
    app.Start (Seconds (0.0));

    //AsciiTraceHelper ascii;
    //channel3.EnableAsciiAll (ascii.CreateFileStream ("hula-trace-example.tr"));

/*    UdpEchoServerHelper echoServer (9);

    ApplicationContainer serverApps = echoServer.Install (m_nodes.Get (0));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (10.0));

    UdpEchoClientHelper echoClient (serverInterface.GetAddress(0), 9);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

    ApplicationContainer clientApps = echoClient.Install (m_nodes.Get (5));
    clientApps.Add(echoClient.Install (m_nodes.Get (3)));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (10.0));*/

    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}


