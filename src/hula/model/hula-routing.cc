/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* Author: Antonio Marsico <antonio.marsico@bt.com> */

#include <iomanip>
#include <cmath>
#include <ns3/internet-module.h>
#include "ns3/queue.h"
#include "hula-routing.h"
#include "ns3/log.h"
#include "ns3/socket-factory.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/node.h"
#include "ns3/ipv4-packet-info-tag.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/names.h"
#include "ns3/packet-socket-address.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/ptr.h"
#include "ns3/abort.h"
#include "ns3/queue.h"
#include "ns3/point-to-point-net-device.h"

namespace ns3 {

    NS_LOG_COMPONENT_DEFINE ("HULARouting");
    NS_OBJECT_ENSURE_REGISTERED (HULARouting);

    /**
     * \ingroup hula
     *
     * \class HULAProcessor
     * \brief Proccess the HULA packets and updates the routing table accordingly
     */

    TypeId HULARouting::GetTypeId(void) {
        static TypeId tid = TypeId ("ns3::HULARouting")
                .SetParent<Ipv4RoutingProtocol> ()
                .AddConstructor<HULARouting>()
                .SetGroupName ("HULA")
                .AddAttribute ("IsEdge",
                               "Set the device as an Edge Device.",
                               BooleanValue (false),
                               MakeBooleanAccessor (&HULARouting::m_isEdge),
                               MakeBooleanChecker ())
                .AddAttribute ("queue",
                               "Use queue depth instead of link congestion",
                               BooleanValue (false),
                               MakeBooleanAccessor (&HULARouting::m_useQueue),
                               MakeBooleanChecker ())
                .AddAttribute ("Tau",
                               "Set the tau estimator value.",
                               DoubleValue (TAU),
                               MakeDoubleAccessor (&HULARouting::m_tau),
                               MakeDoubleChecker<double> ())
                .AddAttribute ("flowletInterval",
                               "Set the flowlet interval value.",
                               DoubleValue (FLOWLET_INTERVAL),
                               MakeDoubleAccessor (&HULARouting::m_flowlet_interval),
                               MakeDoubleChecker<double> ())
        ;
        return tid;
    }

    HULARouting::HULARouting() {
        m_socket = nullptr;
    }

    HULARouting::~HULARouting() {

    }

    void
    HULARouting::HandleRead (Ptr<Socket> socket)
    {

        NS_LOG_FUNCTION (this << socket);

        HULAHeader header;
        Ptr<Packet> packet = 0;
        Address sender;
        packet = m_socket->RecvFrom (sender);

        InetSocketAddress senderAddr = InetSocketAddress::ConvertFrom (sender);
        NS_LOG_LOGIC ("Received " << *packet << " from " << senderAddr.GetIpv4 ());

        //Ipv4Address senderAddress = senderAddr.GetIpv4 ();
        //uint16_t senderPort = senderAddr.GetPort ();

        Ipv4PacketInfoTag interfaceInfo;
        if (!packet->RemovePacketTag (interfaceInfo))
        {
            NS_ABORT_MSG ("No incoming interface on HULA message, aborting.");
        }
        uint32_t incomingIf = interfaceInfo.GetRecvIf ();

        Ptr<Node> node = this->GetObject<Node> ();
        Ptr<NetDevice> dev = node->GetDevice (incomingIf);
        uint32_t ipInterfaceIndex = (uint32_t)m_ipv4->GetInterfaceForDevice (dev);

        if (packet->RemoveHeader (header) == 0)
        {
            return;
        } else {
            NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s node " << node->GetId() << " received HULA Packet from port " <<
            ipInterfaceIndex << " " << header);

            ProcessPacket(packet, header, ipInterfaceIndex);
        }

    }

    void HULARouting::ProcessPacket(Ptr<Packet> packet, HULAHeader header, uint32_t incoming_interface) {

        //Avoid the probe that comes back from an aggregate switch to a core in the first run.
        //TODO: Think sth better

        NS_LOG_LOGIC("Processing HULA Packet");

        if (m_ipv4->GetNInterfaces() == 3) {
            int32_t current_out_port = GetRouteOutPort(header);
            if (current_out_port != -1) {
                if ((uint32_t)current_out_port != incoming_interface) {
                    return;
                }
            }
        }

        // Calculate and update link utilization.
        //uint32_t hula_packet_size = header.GetSerializedSize() + packet->GetSize() + 40; //standard MAC + IP header
        uint32_t hula_packet_size = packet->GetSize();
        AddOrUpdateHULAInfo(incoming_interface, hula_packet_size);
        HULAChannelInfo* incomingChannelInfo = GetInterfaceHULAInfo(incoming_interface);

        if (incomingChannelInfo != nullptr) {
                if (!IsDeniedAddress(incomingChannelInfo, header.getDestination())) {
                    Ipv4Address *address = new Ipv4Address(header.getDestination());

                    incomingChannelInfo->deniedProbeAddresses.push_back(address);
                }
        }

        uint64_t link_utilization_local = 0;

        if (m_useQueue) {

            Ptr<NetDevice> currentNetDevice = m_ipv4->GetNetDevice(incoming_interface);
            Ptr<PointToPointNetDevice> p2pNetDevice = DynamicCast<PointToPointNetDevice>(currentNetDevice);

            NS_ASSERT(p2pNetDevice != nullptr);

            Ptr<Queue> queue = p2pNetDevice->GetQueue ();
            link_utilization_local = std::max((uint32_t)header.getMax_util_(), queue->GetNPackets());

        } else {
            double channel_util_power = incomingChannelInfo->util * 1000;
            uint64_t current_utilization_int = round(channel_util_power);
            link_utilization_local = std::max(header.getMax_util_(), current_utilization_int);
        }

        link_utilization_local = AddOrUpdateNetworkRoute(header.getDestination(),
                                header.getSubnetMask(), incoming_interface,
                                link_utilization_local,
                                header.GetTime());

        NS_LOG_LOGIC("Link utilization " << link_utilization_local);

        Ptr<Socket> sendingSocket;
        for (SocketListI iter = m_sendSocketList.begin(); iter != m_sendSocketList.end(); iter++) {
            if (iter->second != incoming_interface) {

                sendingSocket = iter->first;
                uint32_t current_interface_index = iter->second;

                HULAChannelInfo* currentChannelInfo = GetInterfaceHULAInfo(current_interface_index);

                bool isDenied = false;

                if (currentChannelInfo != nullptr) {
                    isDenied = IsDeniedAddress(currentChannelInfo, header.getDestination());
                }

                if (isProbeAllowed(header.getDestination(), incoming_interface) && !isDenied) {

                    Ptr<Packet> newPacket = Create<Packet>();
                    HULAHeader hulaHeader;
                    hulaHeader.SetTime(header.GetTime());
                    hulaHeader.setMax_util_(link_utilization_local);
                    hulaHeader.setDestination(header.getDestination());
                    hulaHeader.setSubnetMask(header.getSubnetMask());
                    newPacket->AddHeader(hulaHeader);

                    if ((sendingSocket->SendTo(newPacket, 0,
                                               InetSocketAddress(Ipv4Address("255.255.255.255"), PORT))) >= 0) {
                        NS_LOG_LOGIC ("HULA Probe sent");
                    } else {
                        NS_LOG_LOGIC ("Error while sending HULA Probe");
                    }
                }

            }
        }
    }

    Ptr<Ipv4Route> HULARouting::RouteOutput(Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif,
                                              Socket::SocketErrno &sockerr) {
        NS_LOG_FUNCTION (this << p << &header << oif << &sockerr);
        //
        // First, see if this is a multicast packet we have a route for.  If we
        // have a route, then send the packet down each of the specified interfaces.
        //

        if (header.GetDestination ().IsMulticast ())
        {
            NS_LOG_LOGIC ("Multicast destination-- returning false");
            return 0; // Let other routing protocols try to handle this
        }
        //
        // See if this is a unicast packet we have a route for.
        //
        NS_LOG_LOGIC ("Unicast destination- looking up");

        uint8_t prot = header.GetProtocol ();
        uint16_t fragOffset = header.GetFragmentOffset ();

        TcpHeader tcpHdr;
        UdpHeader udpHdr;
        uint16_t srcPort = 0;
        uint16_t destPort = 0;

        if (prot == 6 && fragOffset == 0) // TCP
        {
            p->PeekHeader (tcpHdr);
            srcPort = tcpHdr.GetSourcePort ();
            destPort = tcpHdr.GetDestinationPort ();
        }
        else if (prot == 17 && fragOffset == 0) // UDP
        {
            p->PeekHeader (udpHdr);
            srcPort = udpHdr.GetSourcePort ();
            destPort = udpHdr.GetDestinationPort ();
        }

        Flowlet *flowlet = new Flowlet(header.GetSource(), header.GetDestination(), header.GetProtocol(),srcPort,destPort);
 
        Ptr<Ipv4Route> rtentry = LookupHULARoute (flowlet, oif);

        if (rtentry)
        {
            sockerr = Socket::ERROR_NOTERROR;
        }
        else
        {
            sockerr = Socket::ERROR_NOROUTETOHOST;
        }
        delete flowlet;
        return rtentry;
    }

    bool HULARouting::RouteInput(Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,
                                   Ipv4RoutingProtocol::UnicastForwardCallback ucb,
                                   Ipv4RoutingProtocol::MulticastForwardCallback mcb,
                                   Ipv4RoutingProtocol::LocalDeliverCallback lcb,
                                   Ipv4RoutingProtocol::ErrorCallback ecb) {
        NS_LOG_FUNCTION (this << p << header << header.GetSource () << header.GetDestination () << idev);
        NS_ASSERT (m_ipv4 != 0);
        // Check if input device supports IP
        NS_ASSERT (m_ipv4->GetInterfaceForDevice (idev) >= 0);
        uint32_t iif = m_ipv4->GetInterfaceForDevice (idev);

        NS_LOG_LOGIC("Input interface " << iif);
        Ipv4Address dst = header.GetDestination ();


        if (m_ipv4->IsDestinationAddress (header.GetDestination (), iif))
        {
            if (!lcb.IsNull ())
            {
                NS_LOG_LOGIC ("Local delivery to " << header.GetDestination ());
                lcb (p, header, iif);
                return true;
            }
            else
            {
                // The local delivery callback is null.  This may be a multicast
                // or broadcast packet, so return false so that another
                // multicast routing protocol can handle it.  It should be possible
                // to extend this to explicitly check whether it is a unicast
                // packet, and invoke the error callback if so
                return false;
            }
        }

        if (dst.IsMulticast ())
        {
            NS_LOG_LOGIC ("Multicast route not supported by HULA");
            return false; // Let other routing protocols try to handle this
        }

        if (header.GetDestination ().IsBroadcast ())
        {
            NS_LOG_LOGIC ("Dropping packet not for me and with dst Broadcast");
            if (!ecb.IsNull ())
            {
                ecb (p, header, Socket::ERROR_NOROUTETOHOST);
            }
            return false;
        }

        // Check if input device supports IP forwarding
        if (m_ipv4->IsForwarding (iif) == false)
        {
            NS_LOG_LOGIC ("Forwarding disabled for this interface");
            if (!ecb.IsNull ())
            {
                ecb (p, header, Socket::ERROR_NOROUTETOHOST);
            }
            return true;
        }
        // Next, try to find a route

        NS_LOG_LOGIC ("Unicast destination");

        uint8_t prot = header.GetProtocol ();
        uint16_t fragOffset = header.GetFragmentOffset ();

        TcpHeader tcpHdr;
        UdpHeader udpHdr;
        uint16_t srcPort = 0;
        uint16_t destPort = 0;

        if (prot == 6 && fragOffset == 0) // TCP
        {
            p->PeekHeader (tcpHdr);
            srcPort = tcpHdr.GetSourcePort ();
            destPort = tcpHdr.GetDestinationPort ();
        }
        else if (prot == 17 && fragOffset == 0) // UDP
        {
            p->PeekHeader (udpHdr);
            srcPort = udpHdr.GetSourcePort ();
            destPort = udpHdr.GetDestinationPort ();
        }

        Flowlet *flowlet = new Flowlet(header.GetSource(), header.GetDestination(), header.GetProtocol(),srcPort,destPort);
        Ptr<Ipv4Route> rtentry = LookupHULARoute (flowlet);
        delete flowlet;

        if (rtentry != 0)
        {
            NS_LOG_LOGIC ("Found unicast destination - calling unicast callback");

            uint32_t packetSize = p->GetSize();
            //add headers
            packetSize += 40;

            uint32_t output_interface = m_ipv4->GetInterfaceForDevice (rtentry->GetOutputDevice());
            AddOrUpdateHULAInfo(output_interface, packetSize);
            ucb (rtentry, p, header);  // unicast forwarding callback
            return true;
        }
        else
        {
            NS_LOG_LOGIC ("Did not find unicast destination - returning false");
            return false; // Let other routing protocols try to handle this
        }
    }

    void HULARouting::NotifyInterfaceUp(uint32_t interface) {

    }

    void HULARouting::NotifyInterfaceDown(uint32_t interface) {

    }

    void HULARouting::NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address) {

    }

    void HULARouting::NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address) {

    }

    void HULARouting::SetIpv4(Ptr<Ipv4> ipv4) {
        NS_LOG_FUNCTION (this << ipv4);
        NS_ASSERT (m_ipv4 == 0 && ipv4 != 0);
        m_ipv4 = ipv4;
    }

    void HULARouting::PrintRoutingTable(Ptr<OutputStreamWrapper> stream) const {
        NS_LOG_FUNCTION (this << stream);
        std::ostream* os = stream->GetStream ();

        *os << "Node: " << m_ipv4->GetObject<Node> ()->GetId ()
            << ", Ipv4GlobalRouting table" << std::endl;

        if (GetNRoutes () > 0)
        {
            *os << "Destination     Gateway         Genmask         Flags Util Ref    Use Iface" << std::endl;
            for (uint32_t j = 0; j < GetNRoutes (); j++)
            {
                std::ostringstream dest, gw, mask, flags, utilization;
                HULARoutingTableEntry *route = GetRoute (j);
                dest << route->GetDest ();
                *os << std::setiosflags (std::ios::left) << std::setw (16) << dest.str ();
                gw << route->GetGateway ();
                *os << std::setiosflags (std::ios::left) << std::setw (16) << gw.str ();
                mask << route->GetDestNetworkMask ();
                *os << std::setiosflags (std::ios::left) << std::setw (16) << mask.str ();
                flags << "U";
                if (route->IsHost ())
                {
                    flags << "H";
                }
                else if (route->IsGateway ())
                {
                    flags << "G";
                }
                *os << std::setiosflags (std::ios::left) << std::setw (6) << flags.str ();
                utilization << route->getMax_util_();
                *os << std::setiosflags (std::ios::left) << std::setw (6) << utilization.str ();
                // Ref ct not implemented
                *os << "-" << "      ";
                // Use not implemented
                *os << "-" << "   ";
                if (Names::FindName (m_ipv4->GetNetDevice (route->GetInterface ())) != "")
                {
                    *os << Names::FindName (m_ipv4->GetNetDevice (route->GetInterface ()));
                }
                else
                {
                    *os << route->GetInterface ();
                }
                *os << std::endl;
            }
        }
        *os << std::endl;
    }


    void HULARouting::DoInitialize(void) {

        NS_LOG_INFO ( "Function HULARouting::DoInitialize called" );

        for (uint32_t i = 0 ; i < m_ipv4->GetNInterfaces (); i++)
        {
            NS_LOG_LOGIC ("HULA: Network-Device " << m_ipv4->GetNetDevice (i));
            Ptr<LoopbackNetDevice> check = DynamicCast<LoopbackNetDevice> (m_ipv4->GetNetDevice (i));
            if (check)
            {
                continue;
            }

            for (uint32_t j = 0; j < m_ipv4->GetNAddresses (i); j++)
            {
                Ipv4InterfaceAddress address = m_ipv4->GetAddress (i, j);
                if (address.GetScope() != Ipv4InterfaceAddress::HOST)
                {
                    NS_LOG_LOGIC ("HULA: adding socket to " << address.GetLocal ());
                    TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
                    Ptr<Node> theNode = GetObject<Node> ();
                    Ptr<Socket> socket = Socket::CreateSocket (theNode, tid);
                    InetSocketAddress local = InetSocketAddress (address.GetLocal ());
                    socket->BindToNetDevice (m_ipv4->GetNetDevice (i));
                    socket->SetAllowBroadcast (true);
                    socket->ShutdownRecv();
                    int ret = socket->Bind (local);
                    NS_ASSERT_MSG (ret == 0, "Bind unsuccessful");
                    m_sendSocketList[socket] = i;
                }
            }
        }

        NS_LOG_FUNCTION (this);

        TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
        Ptr<Node> theNode = GetObject<Node> ();
        m_socket = Socket::CreateSocket (theNode, tid);
        InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), PORT);
        m_socket->SetAllowBroadcast (true);
        m_socket->Bind (local);
        m_socket->SetRecvPktInfo (true);
        m_socket->SetRecvCallback (MakeCallback (&HULARouting::HandleRead, this));

        Ipv4RoutingProtocol::DoInitialize();
    }

    void HULARouting::DoDispose(void) {

        NS_LOG_FUNCTION (this);

        if (m_socket != nullptr)
        {
            m_socket->Close ();
            m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
            m_socket = nullptr;
        }

        for (NetworkRoutesI j = m_networkRoutes.begin ();
             j != m_networkRoutes.end ();
             j = m_networkRoutes.erase (j))
        {
            delete (*j);
        }

        for (SocketListI iter = m_sendSocketList.begin (); iter != m_sendSocketList.end (); iter++ )
        {
            iter->first->Close ();
        }
        m_sendSocketList.clear ();

        for (ChannelInfoI j = m_channel_info_list.begin ();
             j != m_channel_info_list.end ();
             j++)
        {
            delete (j->second);
        }

        m_channel_info_list.clear();

        for (FlowletListI j = m_flowlet_list.begin ();
             j != m_flowlet_list.end ();
             j = m_flowlet_list.erase (j))
        {
            delete (*j);
        }

        Ipv4RoutingProtocol::DoDispose();
    }

    void
    HULARouting::AddNetworkRoute(Ipv4Address network, Ipv4Mask networkMask, uint32_t interface) {

        NS_LOG_FUNCTION (this << network << networkMask << interface);

        HULARoutingTableEntry *route = new HULARoutingTableEntry(network, networkMask, interface, 0, 0);
        m_networkRoutes.push_back (route);
    }

    uint32_t
    HULARouting::GetNRoutes (void) const
    {
        NS_LOG_FUNCTION (this);
        uint32_t n = 0;
        n += m_networkRoutes.size ();
        return n;
    }

    Ptr<Ipv4Route> HULARouting::LookupHULARoute(Flowlet *flowlet, Ptr<NetDevice> oif) {

        Ipv4Address dest = flowlet->getM_dst_address();
        NS_LOG_FUNCTION (this << dest << oif);
        NS_LOG_LOGIC ("Looking for route for destination " << dest);
        Ptr<Ipv4Route> rtentry = 0;
        // store all available routes that bring packets to their destination
        typedef std::vector<HULARoutingTableEntry*> RouteVec_t;
        RouteVec_t allRoutes;

        NS_LOG_LOGIC ("Number of m_networkRoutes " << m_networkRoutes.size ());


        for (NetworkRoutesI j = m_networkRoutes.begin ();
             j != m_networkRoutes.end ();
             j++)
        {
            Ipv4Mask mask = (*j)->GetDestNetworkMask ();
            Ipv4Address entry = (*j)->GetDestNetwork ();
            if (mask.IsMatch (dest, entry))
            {
                if (oif != 0)
                {
                    if (oif != m_ipv4->GetNetDevice ((*j)->GetInterface ()))
                    {
                        NS_LOG_LOGIC ("Not on requested interface, skipping");
                        continue;
                    }
                }
                NS_LOG_LOGIC ( "Route -- Network: " << (*j) -> GetDestNetwork () << " Mask: " << (*j) -> GetDestNetworkMask ()
                    << " Interface: " << (*j) -> GetInterface() << " Util: " << (*j) -> getMax_util_() << (*j) -> getTimeStamp());
                allRoutes.push_back (*j);
                NS_LOG_LOGIC (allRoutes.size () << " Found global network route, out interface " << (*j)->GetInterface ());
            }
        }

        if (allRoutes.size () > 0 ) // if route(s) is found
        {

            //Only one HULA route is stored at time
            uint32_t selectIndex = 0;
            Ipv4RoutingTableEntry* route = allRoutes.at (selectIndex);
            // create a Ipv4Route object from the selected routing table entry
            rtentry = Create<Ipv4Route> ();
            rtentry->SetDestination (route->GetDest ());
            /// \todo handle multi-address case
            rtentry->SetSource (m_ipv4->GetAddress (route->GetInterface (), 0).GetLocal ());
            rtentry->SetGateway (route->GetGateway ());

            int32_t interfaceIdx = GetFlowletOutPort(flowlet);

            if (interfaceIdx == -1) {
                interfaceIdx = route->GetInterface ();
            }

            flowlet->setLast_out_interface(interfaceIdx);

            AddOrUpdateFlowlet(flowlet);

            rtentry->SetOutputDevice (m_ipv4->GetNetDevice ((uint32_t) interfaceIdx));
            return rtentry;
        }
        else
        {
            return 0;
        }
    }

    HULARoutingTableEntry *HULARouting::GetRoute(uint32_t index) const {
        NS_LOG_FUNCTION (this << index);

        uint32_t tmp = 0;
        if (index < m_networkRoutes.size ())
        {
            for (NetworkRoutesCI j = m_networkRoutes.begin ();
                 j != m_networkRoutes.end ();
                 j++)
            {
                if (tmp == index)
                {
                    return *j;
                }
                tmp++;
            }
        }

        return nullptr;
    }

    uint64_t HULARouting::AddOrUpdateNetworkRoute(Ipv4Address network, Ipv4Mask networkMask,
            uint32_t interface, uint64_t utilization, uint64_t timeStamp) {

        bool routeFound = false;

        if (!isLocalAddress(network)) {
            if (GetNRoutes() > 0) {
                for (uint32_t j = 0; j < GetNRoutes(); j++) {
                    HULARoutingTableEntry *route = GetRoute(j);

                    Ipv4Mask mask = route->GetDestNetworkMask();
                    Ipv4Address entry = route->GetDestNetwork();
                    uint32_t stored_utilization = route->getMax_util_();

                    if (mask.IsMatch(network, entry)) {

                        routeFound = true;

                        if (interface == route->GetInterface()) {
                            if (stored_utilization != utilization) {

                                route->setMax_util_(utilization);
                                route->setTimeStamp(timeStamp);
                                NS_LOG_LOGIC("Utilization updated");

                            }
                        } else {
                            if (stored_utilization > utilization) {
                                route->setMax_util_(utilization);
                                route->setTimeStamp(timeStamp);
                                route->SetInterface(interface);
                                NS_LOG_LOGIC("Route updated");
                            } else {
                                NS_LOG_LOGIC("Returning old route utilization");
                                return route->getMax_util_();
                            }
                        }

                    }
                }
            }

            /*if (updateRoute) {

                NS_LOG_INFO("Route updated: IP dst " << network << " OutInterface " << interface);

                DeleteRoute(routeID);

                HULARoutingTableEntry *route = new HULARoutingTableEntry(network, networkMask, interface, utilization, timeStamp);
                m_networkRoutes.push_back(route);
            }*/

            if (!routeFound) {
                NS_LOG_INFO("New Route found: IP dst " << network << " OutInterface " << interface);
                HULARoutingTableEntry *route = new HULARoutingTableEntry(network, networkMask, interface, utilization, timeStamp);
                m_networkRoutes.push_back(route);
            }

            return utilization;
        }

        return 0;



    }

    bool HULARouting::isLocalAddress(Ipv4Address ipv4Address) {

        for (uint32_t i = 0; i < m_ipv4->GetNInterfaces(); i++) {
            Ptr<LoopbackNetDevice> check = DynamicCast<LoopbackNetDevice> (m_ipv4->GetNetDevice (i));
            if (check)
            {
                continue;
            }

            Ipv4InterfaceAddress address = m_ipv4->GetAddress (i, 0);
            if (m_ipv4->GetInterfaceForPrefix(ipv4Address, address.GetMask()) != -1) {
                return true;
            }

        }

        return false;
    }

    bool HULARouting::isProbeAllowed(Ipv4Address ipv4Address, uint32_t incoming_interface) {

        if (isLocalAddress(ipv4Address)) {
            for (uint32_t i = 0; i < m_ipv4->GetNInterfaces(); i++) {
                Ptr<LoopbackNetDevice> check = DynamicCast<LoopbackNetDevice> (m_ipv4->GetNetDevice (i));
                if (check)
                {
                    continue;
                }

                Ipv4InterfaceAddress address = m_ipv4->GetAddress (i, 0);
                int32_t addressInterface = m_ipv4->GetInterfaceForPrefix(ipv4Address, address.GetMask());

                if (addressInterface == -1) {
                    continue;
                }

                if ((uint32_t)addressInterface == incoming_interface) {
                    return true;
                }

            }

            return false;
        } else {
            return !m_isEdge;
        }
    }

    void HULARouting::DeleteRoute(uint32_t index) {
        NS_LOG_FUNCTION (this << index);

        uint32_t tmp = 0;
        if (index < m_networkRoutes.size ())
        {
            for (NetworkRoutesCI j = m_networkRoutes.begin ();
                 j != m_networkRoutes.end ();
                 j++)
            {
                if (tmp == index)
                {
                    delete *j;
                    m_networkRoutes.erase(j);
                    return;
                }
                tmp++;
            }
        }
    }

    int32_t HULARouting::GetRouteOutPort(HULAHeader header) {

        if (GetNRoutes () > 0) {
            for (uint32_t j = 0; j < GetNRoutes(); j++) {
                HULARoutingTableEntry *route = GetRoute(j);

                Ipv4Mask mask = route->GetDestNetworkMask();
                Ipv4Address entry = route->GetDestNetwork();

                Ipv4Address ipv4AddressProbeAddress = header.getDestination();

                if (mask.IsMatch(ipv4AddressProbeAddress, entry)) {
                        return route->GetInterface();
                }

            }
        }
        return -1;
    }

    HULAChannelInfo *HULARouting::GetInterfaceHULAInfo(uint32_t ipv4_interface) {

        for (ChannelInfoI iter = m_channel_info_list.begin (); iter != m_channel_info_list.end (); iter++ ) {
            if (iter->first == ipv4_interface) {
                return iter->second;
            }
        }

        return nullptr;
    }

    void HULARouting::AddOrUpdateHULAInfo(uint32_t ipv4_interface, uint32_t packetSize) {

        double currentTime = Simulator::Now().GetSeconds();

        //printf("%.7lf\n",currentTime);

        HULAChannelInfo *channelInfo = GetInterfaceHULAInfo(ipv4_interface);

        if (channelInfo == nullptr) {

            channelInfo = new HULAChannelInfo();
            channelInfo->lastSeenTime = currentTime;
            channelInfo->util = packetSize;


        } else {
            double deltaTime = currentTime - channelInfo->lastSeenTime;

            double estimator = 0.0;

            if (deltaTime < m_tau) {
                //Avoid negative values when the traffic is too slow!!
                estimator = (1.0 - (deltaTime / m_tau));
                NS_LOG_LOGIC("Estimator " << estimator);
            } else {
                NS_LOG_LOGIC("Estimator " << estimator);
            }

            double linkUtilization = channelInfo->util * estimator;

            channelInfo->util = linkUtilization + packetSize;

            channelInfo->lastSeenTime = currentTime;
        }

        m_channel_info_list[ipv4_interface] = channelInfo;
    }

    int32_t HULARouting::GetFlowletOutPort(Flowlet *flowlet) {

        double currentTime = flowlet->getLastSeenTime().GetSeconds();

        Flowlet *databaseFlowlet = GetFlowlet(flowlet->getHash());

        if (databaseFlowlet != nullptr) {

            double last_seen_time = databaseFlowlet->getLastSeenTime().GetSeconds();

            if (currentTime - last_seen_time > m_flowlet_interval) {
                return -1;
            } else {
                return databaseFlowlet->getLast_out_interface();
            }
        }

        return -1;
    }

    /**
        Returns the flowlet which matches the hash otherwise returns
        nullptr.
     **/
    Flowlet *HULARouting::GetFlowlet(uint32_t hash) {

        NS_LOG_FUNCTION (this << hash);

        if (m_flowlet_list.size () > 0)
        {
            for (FlowletListI j = m_flowlet_list.begin ();
                 j != m_flowlet_list.end ();
                 j++)
            {
                if ((*j)->getHash() == hash)
                {
                    return *j;
                }
            }
        }

        return nullptr;
    }


    /**
        FLOWLET TABLE = DICT{}:
        FLOWLET_TABLE[HASH(FLOWLET)] = (LAST_SEEN, NEXT HOP) 
    **/
    void HULARouting::AddOrUpdateFlowlet(Flowlet *flowlet) {

        double currentTime = flowlet->getLastSeenTime().GetSeconds();

        Flowlet *databaseFlowlet = GetFlowlet(flowlet->getHash());

        if (databaseFlowlet != nullptr) {

            double last_seen_time = databaseFlowlet->getLastSeenTime().GetSeconds();

            databaseFlowlet->setLastSeenTime(flowlet->getLastSeenTime());

            if (currentTime - last_seen_time > m_flowlet_interval) {
                databaseFlowlet->setLast_out_interface(flowlet->getLast_out_interface());
            }

        } else {
            Flowlet *copyFlowlet = new Flowlet(flowlet);
            m_flowlet_list.push_back(copyFlowlet);
        }

    }

    void HULARouting::DeleteFlowlet(uint32_t hash) {

    }

    bool HULARouting::IsDeniedAddress(HULAChannelInfo *channelInfo, Ipv4Address address) {

        if (!channelInfo->deniedProbeAddresses.empty()) {
            for (auto &deniedProbeAddress : channelInfo->deniedProbeAddresses) {

                if ((*deniedProbeAddress) == address) {
                    return true;
                }

            }
        }

        return false;
    }

    //Implementation of HULA routing table entry


    HULARoutingTableEntry::HULARoutingTableEntry()
        : max_util_(0), timeStamp(0) {
    }

    HULARoutingTableEntry::~HULARoutingTableEntry() {

    }

    HULARoutingTableEntry::HULARoutingTableEntry(Ipv4Address network, Ipv4Mask networkPrefix,
                                                uint32_t interface, uint64_t utilization, uint64_t timeStamp)
            : Ipv4RoutingTableEntry(Ipv4RoutingTableEntry::CreateNetworkRouteTo(network, networkPrefix, interface)),
              max_util_(utilization),
              timeStamp(timeStamp),
              m_interface(interface)
    {
        NS_LOG_FUNCTION (this << network << networkPrefix << interface << utilization << timeStamp);

        Ipv4RoutingTableEntry route = Ipv4RoutingTableEntry::CreateNetworkRouteTo(network, networkPrefix, interface);
        NS_LOG_FUNCTION ("Route: " << route);

    }

    uint64_t HULARoutingTableEntry::getMax_util_() const {
        return max_util_;
    }

    void HULARoutingTableEntry::setMax_util_(uint64_t max_util_) {
        HULARoutingTableEntry::max_util_ = max_util_;
    }

    uint64_t HULARoutingTableEntry::getTimeStamp() const {
        return timeStamp;
    }

    void HULARoutingTableEntry::setTimeStamp(uint64_t timeStamp) {
        HULARoutingTableEntry::timeStamp = timeStamp;
    }
}

