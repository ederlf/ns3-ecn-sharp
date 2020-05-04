/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* Author: Antonio Marsico <antonio.marsico@bt.com> */

// Include a header file from your module to test.
#include <ns3/boolean.h>
#include <ns3/config.h>
#include <ns3/double.h>
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/hulaProbeGenerator.h"
#include "ns3/hula-generator-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/hula-routing-helper.h"
#include "ns3/hula-routing.h"
#include "ns3/ipv4-global-routing.h"
#include "ns3/log.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/socket-factory.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/ipv4-static-routing.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-packet-info-tag.h"
#include "ns3/rpc-generator-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/rpc-application.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
// An essential include is test.h
#include "ns3/test.h"
#include <iostream>

/**
 *           n0
 *           | net1
 *           n1
 *          / \
 *  net2   /  \ net3
 *        n2  n3
 *  net4   \   /  net5
 *         \ /
 *         n4
 *         |  net6
 *         n5
 */

// Do not put your test classes in namespace ns3.  You may find it useful
// to use the using directive to access the ns3 namespace directly
using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HULATestSuite");

// This is an example TestCase.
class HulaRoutingTestProbeTest : public TestCase
{
public:
  HulaRoutingTestProbeTest ();
  virtual ~HulaRoutingTestProbeTest ();

    Ptr<Packet> m_receivedPacket; //!< number of received packets

    /**
     * \brief Receive a packet.
     * \param socket The receiving socket.
     */
    void ReceivePkt (Ptr<Socket> socket);
    /**
     * \brief Send a packet.
     * \param socket The sending socket.
     * \param to The address of the receiver.
     */
    void DoSendData (Ptr<Socket> socket, std::string to);
    /**
     * \brief Send a packet.
     * \param socket The sending socket.
     * \param to The address of the receiver.
     */
    void SendData (Ptr<Socket> socket, std::string to);

private:
  virtual void DoRun (void);
};

// Add some help text to this case to describe what it is intended to test
HulaRoutingTestProbeTest::HulaRoutingTestProbeTest ()
  : TestCase ("Routing test based on HULA probes")
{
}

// This destructor does nothing but we include it as a reminder that
// the test case should clean up after itself
HulaRoutingTestProbeTest::~HulaRoutingTestProbeTest ()
{
}

void
HulaRoutingTestProbeTest::ReceivePkt (Ptr<Socket> socket)
{
    uint32_t availableData;
    availableData = socket->GetRxAvailable ();
    m_receivedPacket = socket->Recv (std::numeric_limits<uint32_t>::max (), 0);
    NS_ASSERT (availableData == m_receivedPacket->GetSize ());
    //cast availableData to void, to suppress 'availableData' set but not used
    //compiler warning
    (void) availableData;
}

void
HulaRoutingTestProbeTest::DoSendData (Ptr<Socket> socket, std::string to)
{
    Address realTo = InetSocketAddress (Ipv4Address (to.c_str ()), 1234);
    NS_TEST_EXPECT_MSG_EQ (socket->SendTo (Create<Packet> (123), 0, realTo),
                           123, "100");
    
}

void
HulaRoutingTestProbeTest::SendData (Ptr<Socket> socket, std::string to)
{
    m_receivedPacket = Create<Packet> ();
    Simulator::ScheduleWithContext (socket->GetNode ()->GetId (), Seconds (1),
                                    &HulaRoutingTestProbeTest::DoSendData, this, socket, to);
    Simulator::Stop (Seconds (2));
    Simulator::Run ();
}


//
// This method is the pure virtual method from class TestCase that every
// TestCase must implement
//
void
HulaRoutingTestProbeTest::DoRun (void)
{

    Config::SetDefault ("ns3::Ipv4GlobalRouting::RandomEcmpRouting", BooleanValue (true));

    NodeContainer m_nodes;
    m_nodes.Create(3);

    NodeContainer edgeNodes;
    edgeNodes.Create(3);

    NodeContainer hostContainer;
    hostContainer.Create(3);

    //connect node0 to node1 and node2
    PointToPointHelper channel1;
    channel1.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
    channel1.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
    NetDeviceContainer net1 = channel1.Install (hostContainer.Get (0), edgeNodes.Get (0));
    NetDeviceContainer net2 = channel1.Install (edgeNodes.Get (0), m_nodes.Get (0));
    NetDeviceContainer net3 = channel1.Install (edgeNodes.Get (0), m_nodes.Get (1));
    NetDeviceContainer net4 = channel1.Install (m_nodes.Get (0), edgeNodes.Get (1));
    NetDeviceContainer net5 = channel1.Install (m_nodes.Get (1), edgeNodes.Get (1));
    NetDeviceContainer net6 = channel1.Install (edgeNodes.Get (1), hostContainer.Get (1));

    NetDeviceContainer net7 = channel1.Install (m_nodes.Get (0), m_nodes.Get (2));
    NetDeviceContainer net8 = channel1.Install (m_nodes.Get (2), edgeNodes.Get (2));
    NetDeviceContainer net9 = channel1.Install (edgeNodes.Get (2), hostContainer.Get (2));


    InternetStackHelper internet;

    // By default, InternetStackHelper adds a static and global routing
    // implementation.  We just want the global for this test.
    Ipv4GlobalRoutingHelper ipv4RoutingHelper;
    //Ipv4StaticRoutingHelper ipv4StaticRoutingHelper;
    HULARoutingHelper hulaRoutingHelper;
    Ipv4ListRoutingHelper list;
    list.Add(hulaRoutingHelper, 10);
    //list.Add(ipv4StaticRoutingHelper, 0);
    list.Add(ipv4RoutingHelper, 1);

    internet.SetRoutingHelper (list);
    internet.Install (m_nodes);

    InternetStackHelper edgeInternet;
    HULARoutingHelper hulaRoutingHelperEdge;
    hulaRoutingHelperEdge.Set("IsEdge", BooleanValue(true));
    Ipv4ListRoutingHelper edgeList;
    edgeList.Add(hulaRoutingHelperEdge, 10);
    edgeList.Add(ipv4RoutingHelper, 1);
    edgeInternet.SetRoutingHelper (edgeList);

    edgeInternet.Install(edgeNodes);


    InternetStackHelper hostInternet;
    hostInternet.Install(hostContainer);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer net1Addresses = address.Assign (net1);

    address.SetBase ("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer net2Addresses = address.Assign (net2);

    address.SetBase ("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer net3Addresses = address.Assign (net3);

    address.SetBase ("10.1.4.0", "255.255.255.0");
    address.Assign (net4);

    address.SetBase ("10.1.5.0", "255.255.255.0");
    address.Assign (net5);

    address.SetBase ("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer net6Addresses = address.Assign (net6);

    address.SetBase ("10.1.7.0", "255.255.255.0");
    address.Assign (net7);

    address.SetBase ("10.1.8.0", "255.255.255.0");
    address.Assign (net8);

    address.SetBase ("10.1.9.0", "255.255.255.0");
    Ipv4InterfaceContainer net9Addresses = address.Assign (net9);

    /*Ptr<Ipv4StaticRouting> staticRouting;
    staticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (m_nodes.Get (0)->GetObject<Ipv4> ()->GetRoutingProtocol ());
    staticRouting->SetDefaultRoute ("10.1.1.2", 1);
    staticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (m_nodes.Get (5)->GetObject<Ipv4> ()->GetRoutingProtocol ());
    staticRouting->SetDefaultRoute ("10.1.6.1", 1);*/

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    HulaGeneratorHelper hulaHelper;

    ApplicationContainer hulaGenerator = hulaHelper.Install(hostContainer);

    hulaGenerator.Start(Seconds(0.1));
    hulaGenerator.Stop(Seconds(0.11));

    //TODO Check on a specific time
    /*Ptr<HULARouting> hulaRouting =
            Ipv4RoutingHelper::GetRouting <HULARouting> (m_nodes.Get (1)->GetObject<Ipv4> ()->GetRoutingProtocol ());

    uint32_t n_hula_routes_node1 = hulaRouting->GetNRoutes();
    NS_LOG_DEBUG ("HULA Routes node1 " << n_hula_routes_node1);
    NS_TEST_ASSERT_MSG_EQ (n_hula_routes_node1, 1, "Error-- not one route");*/

    HULARoutingHelper g;
    Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("dynamic-global-routing.routes", std::ios::out);
    g.PrintRoutingTableAllAt (Seconds (1), routingStream);

    //AsciiTraceHelper ascii;
    //channel1.EnableAsciiAll (ascii.CreateFileStream ("hula-trace-example.tr"));

    /*NS_LOG_INFO ("Create Applications.");
    uint16_t port = 9;   // Discard port (RFC 863)

    OnOffHelper onoff ("ns3::TcpSocketFactory",
                       Address (InetSocketAddress (net6Addresses.GetAddress(1), port)));
    onoff.SetConstantRate (DataRate ("3Mb/s"));

    ApplicationContainer app = onoff.Install (m_nodes.Get (0));
    // Start the application
    app.Start (Seconds (1.1));
    app.Stop (Seconds (2.0));

    // Create an optional packet sink to receive these packets
    PacketSinkHelper sink ("ns3::TcpSocketFactory",
                           Address (InetSocketAddress (Ipv4Address::GetAny (), port)));
    app = sink.Install (m_nodes.Get (5));
    app.Start (Seconds (0.0));*/


    // Create the UDP sockets
    Ptr<SocketFactory> rxSocketFactory = hostContainer.Get (1)->GetObject<UdpSocketFactory> ();
    Ptr<Socket> rxSocket = rxSocketFactory->CreateSocket ();
    NS_TEST_EXPECT_MSG_EQ (rxSocket->Bind (InetSocketAddress (net6Addresses.GetAddress(1), 1234)), 0, "trivial");
    rxSocket->SetRecvCallback (MakeCallback (&HulaRoutingTestProbeTest::ReceivePkt, this));
    Ptr<SocketFactory> txSocketFactory = hostContainer.Get (0)->GetObject<UdpSocketFactory> ();
    Ptr<Socket> txSocket = txSocketFactory->CreateSocket ();
    txSocket->SetAllowBroadcast (true);

    // ------ Now the tests ------------

    // Unicast test
    SendData (txSocket, "10.1.6.2");
    NS_TEST_EXPECT_MSG_EQ (m_receivedPacket->GetSize (), 123, "HULA routing with /32 did not deliver all packets.");
    Simulator::Destroy ();
}

class HulaTestProbeManagement : public TestCase
{
public:
    HulaTestProbeManagement ();
    virtual ~HulaTestProbeManagement ();

    static const int PORT = 43333;
    /**
     * \brief Send a packet.
     * \param socket The sending socket.
     * \param to The address of the receiver.
     */
    void DoSendData (Ptr<Socket> socket, uint64_t util, std::string to);
    /**
     * \brief Send a packet.
     * \param socket The sending socket.
     * \param to The address of the receiver.
     */
    void SendHULAPacket (Ptr<Socket> socket, uint64_t util, std::string to, double time);

private:
    virtual void DoRun (void);
};

// Add some help text to this case to describe what it is intended to test
HulaTestProbeManagement::HulaTestProbeManagement ()
        : TestCase ("Check HULA routing table based on probes")
{
}

// This destructor does nothing but we include it as a reminder that
// the test case should clean up after itself
HulaTestProbeManagement::~HulaTestProbeManagement ()
{
}

void
HulaTestProbeManagement::DoSendData (Ptr<Socket> socket, uint64_t util, std::string to)
{
    Address realTo = InetSocketAddress (Ipv4Address ("255.255.255.255"), PORT);

    Ptr<Packet> hulaPacket = Create<Packet>();
    HULAHeader hulaHeader;
    hulaHeader.SetTime();
    hulaHeader.setSubnetMask(Ipv4Mask("/32").Get());
    hulaHeader.setMax_util_(util);
    hulaHeader.setDestination(Ipv4Address(to.c_str()));

    hulaPacket->AddHeader(hulaHeader);

    NS_TEST_EXPECT_MSG_EQ (socket->SendTo (hulaPacket, 0, realTo),
                           24, "100");
}

void
HulaTestProbeManagement::SendHULAPacket (Ptr<Socket> socket, uint64_t util, std::string to, double time)
{
    Simulator::ScheduleWithContext (socket->GetNode ()->GetId (), Seconds (time),
                                    &HulaTestProbeManagement::DoSendData, this, socket, util, to);
    Simulator::Stop (Seconds (2));
    Simulator::Run ();
}


//
// This method is the pure virtual method from class TestCase that every
// TestCase must implement
//
void
HulaTestProbeManagement::DoRun (void)
{
    NodeContainer m_nodes;
    m_nodes.Create(3);

    //connect node0 to node1 and node2
    PointToPointHelper channel1;
    channel1.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
    channel1.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

    NetDeviceContainer net1 = channel1.Install (m_nodes.Get (0), m_nodes.Get (1));
    NetDeviceContainer net2 = channel1.Install (m_nodes.Get (0), m_nodes.Get (2));

    InternetStackHelper internet;
    // By default, InternetStackHelper adds a static and global routing
    // implementation.  We just want the global for this test.
    Ipv4GlobalRoutingHelper ipv4RoutingHelper;
    //Ipv4StaticRoutingHelper ipv4StaticRoutingHelper;
    HULARoutingHelper hulaRoutingHelper;
    Ipv4ListRoutingHelper list;
    list.Add(hulaRoutingHelper, 10);
    //list.Add(ipv4StaticRoutingHelper, 0);
    list.Add(ipv4RoutingHelper, 1);
    internet.SetRoutingHelper (list);

    internet.Install (m_nodes);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer net1Addresses = address.Assign (net1);

    address.SetBase ("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer net2Addresses = address.Assign (net2);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    Ptr<SocketFactory> txSocketFactoryN1 = m_nodes.Get (1)->GetObject<UdpSocketFactory> ();
    Ptr<Socket> txSocketN1 = txSocketFactoryN1->CreateSocket ();
    txSocketN1->SetAllowBroadcast (true);

    Ptr<SocketFactory> txSocketFactoryN2 = m_nodes.Get (2)->GetObject<UdpSocketFactory> ();
    Ptr<Socket> txSocketN2 = txSocketFactoryN2->CreateSocket ();
    txSocketN2->SetAllowBroadcast (true);

    // ------ Now the tests ------------

    // Unicast test
    SendHULAPacket (txSocketN1, 60, "10.1.6.2", 1.1);
    SendHULAPacket (txSocketN2, 50, "10.1.6.2", 1.2);

    Ptr<HULARouting> hulaRouting =
            Ipv4RoutingHelper::GetRouting <HULARouting> (m_nodes.Get (0)->GetObject<Ipv4> ()->GetRoutingProtocol ());
    HULARoutingTableEntry *routingTableEntry = hulaRouting->GetRoute(0);

    NS_TEST_EXPECT_MSG_EQ (routingTableEntry->GetDest(), Ipv4Address("10.1.6.2"), "IP address not correct");
    //NS_TEST_EXPECT_MSG_EQ (routingTableEntry->GetInterface(), 2, "Interface not correct " << routingTableEntry->GetInterface());


    SendHULAPacket (txSocketN1, 30, "10.1.6.2", 1.3);
    SendHULAPacket (txSocketN2, 80, "10.1.6.2", 1.4);

    NS_TEST_EXPECT_MSG_EQ (routingTableEntry->GetInterface(), 1, "Interface not correct " << routingTableEntry->GetInterface());

    Simulator::Destroy ();

}

class HULATestFlowlet : public TestCase
{
public:
    HULATestFlowlet ();
    virtual ~HULATestFlowlet ();

    Ptr<Packet> m_receivedPacket; //!< number of received packets

    /**
    * \brief Send some data
    * \param index Index of the socket to use.
    */
    void SendData ();

    /**
    * \brief Shutdown a socket
    * \param index Index of the socket to close.
    */
    void ShutDownSock ();

    void EnableSocket (std::string data_rate);


    void HandleRead (Ptr<Socket> socket);

    static const int PORT = 43333;
    /**
     * \brief Send a packet.
     * \param socket The sending socket.
     * \param to The address of the receiver.
     */
    void DoSendData (Ptr<Socket> socket, uint64_t util, std::string to);
    /**
     * \brief Send a packet.
     * \param socket The sending socket.
     * \param to The address of the receiver.
     */
    void SendHULAPacket (Ptr<Socket> socket, uint64_t util, std::string to, double time);


private:
    virtual void DoRun (void);

    std::pair<Ptr<Socket>, bool> send_socket;

    uint16_t m_count; //!< Number of packets received.
    uint16_t m_packetSize;  //!< Packet size.
    DataRate m_dataRate;  //!< Data rate.
    std::vector<uint8_t > m_firstInterface;  //!< Packets received on the 1st interface at a given time.
    std::vector<uint8_t > m_secondInterface;  //!< Packets received on the 2nd interface at a given time.
};

// Add some help text to this case to describe what it is intended to test
HULATestFlowlet::HULATestFlowlet ()
        : TestCase ("Testing HULA flowlets")
{
    m_firstInterface.resize (16);
    m_secondInterface.resize (16);
    m_dataRate = DataRate ("6kbps");
    m_packetSize = 50;
    m_count = 0;
}

// This destructor does nothing but we include it as a reminder that
// the test case should clean up after itself
HULATestFlowlet::~HULATestFlowlet ()
{
    send_socket.second = false;
    send_socket.first->Close();
    send_socket.first=0;
}

void
HULATestFlowlet::SendData ()
{

    if (send_socket.second == false)
    {
        return;
    }

    Ptr<Packet> packet = Create<Packet> (m_packetSize);
    send_socket.first->Send (packet);

    Time tNext (MicroSeconds (m_packetSize * 8 * 1e6 / m_dataRate.GetBitRate ()));
    Simulator::Schedule (tNext, &HULATestFlowlet::SendData, this);
}

void HULATestFlowlet::ShutDownSock() {

    send_socket.second = false;
    //send_socket.first->Close();
}

void HULATestFlowlet::EnableSocket(std::string data_rate) {

    send_socket.second = true;
    m_dataRate = DataRate (std::move(data_rate));
    //m_dataRate = DataRate ("2kbps");
    //send_socket.first->Close();
}

void
HULATestFlowlet::HandleRead (Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom (from)))
    {
        if (packet->GetSize () == 0)
        { //EOF
            break;
        }
        Ipv4PacketInfoTag tag;
        bool found;
        found = packet->PeekPacketTag (tag);
        uint8_t now = static_cast<uint8_t> (Simulator::Now ().GetSeconds ());
        
        if (found)
        {
            NS_LOG_INFO("Interface " << tag.GetRecvIf());

            if (tag.GetRecvIf () == 0)
            {
                m_firstInterface[now]++;
            }
            if (tag.GetRecvIf () == 1)
            {
                m_secondInterface[now]++;
            }
            m_count++;
        }
    }
}

void
HULATestFlowlet::DoSendData (Ptr<Socket> socket, uint64_t util, std::string to)
{
    Address realTo = InetSocketAddress (Ipv4Address ("255.255.255.255"), PORT);

    Ptr<Packet> hulaPacket = Create<Packet>();
    HULAHeader hulaHeader;
    hulaHeader.SetTime();
    hulaHeader.setSubnetMask(Ipv4Mask("/32").Get());
    hulaHeader.setMax_util_(util);
    hulaHeader.setDestination(Ipv4Address(to.c_str()));

    hulaPacket->AddHeader(hulaHeader);

    NS_TEST_EXPECT_MSG_EQ (socket->SendTo (hulaPacket, 0, realTo),
                           24, "100");
}

void
HULATestFlowlet::SendHULAPacket (Ptr<Socket> socket, uint64_t util, std::string to, double time)
{
    Simulator::ScheduleWithContext (socket->GetNode ()->GetId (), Seconds (time),
                                    &HULATestFlowlet::DoSendData, this, socket, util, to);
}

//
// This method is the pure virtual method from class TestCase that every
// TestCase must implement
//
void
HULATestFlowlet::DoRun (void)
{

    Config::SetDefault ("ns3::HULARouting::flowletInterval", DoubleValue(0.1));
    Config::SetDefault ("ns3::HULARouting::Tau", DoubleValue(0.25));

    NodeContainer m_nodes;
    m_nodes.Create(4);

    NodeContainer hostContainer;
    hostContainer.Create(2);

    //connect node0 to node1 and node2
    PointToPointHelper channel1;
    channel1.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
    channel1.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (200)));
    NetDeviceContainer net1 = channel1.Install (hostContainer.Get (0), m_nodes.Get (0));
    NetDeviceContainer net2 = channel1.Install (m_nodes.Get (0), m_nodes.Get (1));
    NetDeviceContainer net3 = channel1.Install (m_nodes.Get (0), m_nodes.Get (2));
    NetDeviceContainer net4 = channel1.Install (m_nodes.Get (1), m_nodes.Get (3));
    NetDeviceContainer net5 = channel1.Install (m_nodes.Get (2), m_nodes.Get (3));
    NetDeviceContainer net6 = channel1.Install (m_nodes.Get (3), hostContainer.Get (1));


    InternetStackHelper internet;
    // By default, InternetStackHelper adds a static and global routing
    // implementation.  We just want the global for this test.
    Ipv4GlobalRoutingHelper ipv4RoutingHelper;
    //Ipv4StaticRoutingHelper ipv4StaticRoutingHelper;
    HULARoutingHelper hulaRoutingHelper;
    Ipv4ListRoutingHelper list;
    list.Add(hulaRoutingHelper, 10);
    //list.Add(ipv4StaticRoutingHelper, 0);
    list.Add(ipv4RoutingHelper, 1);
    internet.SetRoutingHelper (list);

    internet.Install (m_nodes.Get(1));
    internet.Install (m_nodes.Get(2));

    InternetStackHelper edgeInternet;
    HULARoutingHelper hulaRoutingHelperEdge;
    hulaRoutingHelperEdge.Set("IsEdge", BooleanValue(true));
    Ipv4ListRoutingHelper edgeList;
    edgeList.Add(hulaRoutingHelperEdge, 10);
    edgeList.Add(ipv4RoutingHelper, 1);
    edgeInternet.SetRoutingHelper (edgeList);

    edgeInternet.Install(m_nodes.Get(0));
    edgeInternet.Install(m_nodes.Get(3));


    InternetStackHelper hostInternet;
    hostInternet.Install(hostContainer);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer net1Addresses = address.Assign (net1);

    address.SetBase ("10.1.2.0", "255.255.255.0");
    address.Assign (net2);

    address.SetBase ("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer net3Addresses = address.Assign (net3);

    address.SetBase ("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer net4Addresses = address.Assign (net4);

    address.SetBase ("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer net5Addresses = address.Assign (net5);

    address.SetBase ("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer net6Addresses = address.Assign (net6);


    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    uint16_t port = 9;

    std::pair<Ptr<Socket>, bool>  sendSockA;

    // Create the UDP sockets
    Ptr<SocketFactory> rxSocketFactory = m_nodes.Get (3)->GetObject<UdpSocketFactory> ();
    Ptr<Socket> rxSocket = rxSocketFactory->CreateSocket ();
    rxSocket->Bind (Address (InetSocketAddress (Ipv4Address::GetAny (), port)));
    rxSocket->SetRecvCallback (MakeCallback (&HULATestFlowlet::HandleRead, this));
    rxSocket->SetRecvPktInfo (true);
    rxSocket->Listen ();
    rxSocket->ShutdownSend ();

    Ptr<SocketFactory> txSocketFactory = hostContainer.Get (0)->GetObject<UdpSocketFactory> ();
    sendSockA.first = txSocketFactory->CreateSocket ();
    sendSockA.first->Bind();
    
    sendSockA.first->Connect(InetSocketAddress (net6Addresses.GetAddress (0), port));
    sendSockA.second = true;

    send_socket = sendSockA;

    // ------ Now the tests ------------

    Simulator::Schedule (Seconds (1.0), &HULATestFlowlet::SendData, this);
    Simulator::Schedule (Seconds (5.0), &HULATestFlowlet::ShutDownSock, this);

    Simulator::Schedule (Seconds (5.5), &HULATestFlowlet::EnableSocket, this, "2Kbps");

    Simulator::Schedule (Seconds (6.0), &HULATestFlowlet::SendData, this);
    Simulator::Schedule (Seconds (10.0), &HULATestFlowlet::ShutDownSock, this);

    Ptr<Node> node3 = m_nodes.Get (3);

    Ptr<Ipv4> ipv4 = node3->GetObject<Ipv4> ();
    //uint32_t interface = (uint32_t) ipv4->GetInterfaceForPrefix(net4Addresses.GetAddress(0), Ipv4Mask("/32"));
    Ptr<SocketFactory> txSocketFactoryN1 = node3->GetObject<UdpSocketFactory> ();
    Ptr<Socket> txSocketN1 = txSocketFactoryN1->CreateSocket ();
    //txSocketN1->BindToNetDevice(ipv4->GetNetDevice(interface));
    txSocketN1->SetAllowBroadcast (true);


    SendHULAPacket (txSocketN1, 0, "10.1.6.1", 0.9);
    SendHULAPacket (txSocketN1, 0, "10.1.6.1", 1.9);
    SendHULAPacket (txSocketN1, 0, "10.1.6.1", 5.0);

    SendHULAPacket (txSocketN1, 0, "10.1.6.1", 8.5);
    SendHULAPacket (txSocketN1, 0, "10.1.6.1", 8.52);

    Simulator::Run ();

    NS_TEST_ASSERT_MSG_EQ (m_count, 81, "HULA routing did not deliver all packets " << m_count);


    NS_TEST_ASSERT_MSG_EQ (m_firstInterface[1], 15, "HULA routing did not deliver all packets " << int(m_firstInterface[4]));
    NS_TEST_ASSERT_MSG_EQ (m_firstInterface[4], 15, "HULA routing routing did not deliver all packets " << int(m_firstInterface[6]));

    //Check if the flowlet is re-routed
    NS_TEST_ASSERT_MSG_EQ (m_secondInterface[7], 5, "HULA routing routing did not deliver all packets " << int(m_secondInterface[7]));
    NS_TEST_ASSERT_MSG_EQ (m_firstInterface[9], 5, "HULA routing routing did not deliver all packets " << int(m_firstInterface[9]));
    Simulator::Destroy ();
}

class HULATestRPCGenerator : public TestCase
{
public:
    HULATestRPCGenerator ();
    virtual ~HULATestRPCGenerator ();


    static const int PORT = 43333;

private:
    virtual void DoRun (void);
};

HULATestRPCGenerator::HULATestRPCGenerator()
        : TestCase ("Testing HULA RPC Generator") {

}

HULATestRPCGenerator::~HULATestRPCGenerator() = default;

void HULATestRPCGenerator::DoRun(void) {
    NodeContainer hostContainer;
    hostContainer.Create(2);
    PointToPointHelper channel1;

    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpDCTCP"));

    channel1.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
    channel1.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
    NetDeviceContainer net1 = channel1.Install (hostContainer);

    InternetStackHelper internet;
    internet.Install(hostContainer);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer net1Addresses = address.Assign (net1);

    uint16_t port = 9;

    RPCGeneratorHelper rpcGeneratorHelper ("ns3::TcpSocketFactory",
            InetSocketAddress(net1Addresses.GetAddress(1), port));
    ApplicationContainer generatorApp =
            rpcGeneratorHelper.Install(hostContainer.Get(0));
    generatorApp.Start (Seconds (0.1));
    generatorApp.Stop (Seconds (10.0));

    PacketSinkHelper sink ("ns3::TcpSocketFactory",
                           InetSocketAddress (Ipv4Address::GetAny (), port));
    ApplicationContainer sinkApps = sink.Install (hostContainer.Get (1));

    sinkApps.Start (Seconds (0.0));
    sinkApps.Stop (Seconds (10.0));

    Simulator::Run();
    Simulator::Destroy();

    Ptr<PacketSink> sink1 = DynamicCast<PacketSink> (sinkApps.Get (0));

    Ptr<RPCApplication> rpcgen = DynamicCast<RPCApplication> (generatorApp.Get (0));

    NS_TEST_ASSERT_MSG_EQ (rpcgen->GetTotalConnections(), 20, "Total connections must be 20 instead of " << rpcgen->GetTotalConnections());
    NS_TEST_ASSERT_MSG_GT(sink1->GetTotalRx (), 1500, "Sink didn't receive any packet");
}

// The TestSuite class names the TestSuite, identifies what type of TestSuite,
// and enables the TestCases to be run.  Typically, only the constructor for
// this class must be defined
//
class HulaTestSuite : public TestSuite
{
public:
  HulaTestSuite ();
};

HulaTestSuite::HulaTestSuite ()
  : TestSuite ("hula", UNIT)
{
  // TestDuration for TestCase can be QUICK, EXTENSIVE or TAKES_FOREVER
  AddTestCase (new HulaRoutingTestProbeTest, TestCase::QUICK);
  AddTestCase (new HulaTestProbeManagement, TestCase::QUICK);
  AddTestCase(new HULATestFlowlet, TestCase::QUICK);
  AddTestCase(new HULATestRPCGenerator, TestCase::QUICK);
}

// Do not forget to allocate an instance of this TestSuite
static HulaTestSuite hulaTestSuite;

