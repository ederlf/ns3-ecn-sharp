/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* Author: Antonio Marsico <antonio.marsico@bt.com> */

#ifndef HULA_PROCESSOR_H
#define HULA_PROCESSOR_H

#include "ns3/inet-socket-address.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "hula_hdr.h"
#include "flowlet.h"
#include <list>
#include <stdint.h>

namespace ns3 {

#define TAU 0.001
#define FLOWLET_INTERVAL 0.0001
#define MAX_LINK_UTIL (1<<30)

    class Socket;
    class Packet;

    struct HULAChannelInfo {
        double lastSeenTime;
        double util;
        std::list<Ipv4Address*> deniedProbeAddresses;

        HULAChannelInfo()
        : lastSeenTime(0.0),
          util(0.0)
        {

        }
    };

    class HULARoutingTableEntry : public Ipv4RoutingTableEntry {
    public:

        HULARoutingTableEntry();

        virtual ~HULARoutingTableEntry();

        /**
        * \brief Constructor
        * \param network network address
        * \param networkPrefix network prefix
        * \param interface interface index
        */

        /*
            Author: Saim Salman
            network / networkPrefix --> destination IP 
            interface --> BestHop
            max_util_ --> Path utilization
            timeStamp --> T_fail (Aging Mechanism)
        */

        HULARoutingTableEntry (Ipv4Address network,
                                Ipv4Mask networkPrefix,
                                uint32_t interface,
                                uint64_t utilization,
                                uint64_t timeStamp);

        uint64_t getMax_util_() const;

        void setMax_util_(uint64_t max_util_);

        uint64_t getTimeStamp() const;

        void setTimeStamp(uint64_t timeStamp);

        void setOutInteface(uint32_t interface);



    private:
        uint64_t max_util_;
        uint64_t timeStamp;
        uint32_t m_interface;
    };

/* ... */
    class HULARouting : public Ipv4RoutingProtocol
    {
    public:
        /**
        * \brief Get the type ID.
        * \return the object TypeId
        */
        static TypeId GetTypeId (void);

        HULARouting();
        ~HULARouting() override;

        uint32_t GetNRoutes (void) const;

        HULARoutingTableEntry *GetRoute (uint32_t index) const;

        Ptr<Ipv4Route>
        RouteOutput(Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr) override;

        bool
        RouteInput(Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev, UnicastForwardCallback ucb,
                   MulticastForwardCallback mcb, LocalDeliverCallback lcb, ErrorCallback ecb) override;

        void NotifyInterfaceUp(uint32_t interface) override;

        void NotifyInterfaceDown(uint32_t interface) override;

        void NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address) override;

        void NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address) override;

        void SetIpv4(Ptr<Ipv4> ipv4) override;

        void PrintRoutingTable(Ptr<OutputStreamWrapper> stream) const override;

    protected:
        void DoDispose(void) override;

        void DoInitialize(void) override;

    private:
        static const int PORT = 43333;

        /// container of Ipv4RoutingTableEntry (routes to networks)
        typedef std::list<HULARoutingTableEntry *> NetworkRoutes;
        /// const iterator of container of Ipv4RoutingTableEntry (routes to networks)
        typedef std::list<HULARoutingTableEntry *>::const_iterator NetworkRoutesCI;
        /// iterator of container of Ipv4RoutingTableEntry (routes to networks)
        typedef std::list<HULARoutingTableEntry *>::iterator NetworkRoutesI;

        NetworkRoutes m_networkRoutes;

        // note: we can not trust the result of socket->GetBoundNetDevice ()->GetIfIndex ();
        // it is dependent on the interface initialization (i.e., if the loopback is already up).
        /// Socket list type
        typedef std::map< Ptr<Socket>, uint32_t> SocketList;
        /// Socket list type iterator
        typedef std::map<Ptr<Socket>, uint32_t>::iterator SocketListI;
        /// Socket list type const iterator
        typedef std::map<Ptr<Socket>, uint32_t>::const_iterator SocketListCI;

        SocketList m_sendSocketList; //!< list of sockets for sending (socket, interface index)

        /// container of Ipv4RoutingTableEntry (routes to networks)
        typedef std::map<uint32_t, HULAChannelInfo* > ChannelInfoList;
        /// const iterator of container of Ipv4RoutingTableEntry (routes to networks)
        typedef std::map<uint32_t, HULAChannelInfo* >::const_iterator ChannelInfoCI;
        /// iterator of container of Ipv4RoutingTableEntry (routes to networks)
        typedef std::map<uint32_t, HULAChannelInfo* >::iterator ChannelInfoI;

        ChannelInfoList m_channel_info_list;

        /// container of Ipv4RoutingTableEntry (routes to networks)
        typedef std::list<Flowlet *> FlowletList;
        /// const iterator of container of Ipv4RoutingTableEntry (routes to networks)
        typedef std::list<Flowlet *>::const_iterator FlowletListCI;
        /// iterator of container of Ipv4RoutingTableEntry (routes to networks)
        typedef std::list<Flowlet *>::iterator FlowletListI;

        FlowletList m_flowlet_list;

        /**
        * \brief Add a network route to the global routing table.
        *
        * \param network The Ipv4Address network for this route.
        * \param networkMask The Ipv4Mask to extract the network.
        * \param interface The network interface index used to send packets to the
        * destination.
        *
        * \see Ipv4Address
        */
        void AddNetworkRoute (Ipv4Address network,
                                Ipv4Mask networkMask,
                                uint32_t interface);

        uint64_t AddOrUpdateNetworkRoute (Ipv4Address network,
                                 Ipv4Mask networkMask,
                                 uint32_t interface,
                                 uint64_t utilization=0,
                                 uint64_t timeStamp=0);

        int32_t GetRouteOutPort(HULAHeader header);

        bool isLocalAddress(Ipv4Address ipv4Address);

        bool isProbeAllowed(Ipv4Address ipv4Address, uint32_t incoming_interface);

        HULAChannelInfo *GetInterfaceHULAInfo(uint32_t ipv4_interface);

        bool IsDeniedAddress(HULAChannelInfo *channelInfo, Ipv4Address address);

        void AddOrUpdateHULAInfo(uint32_t ipv4_interface, uint32_t packetSize);

        /**
        * \brief Lookup in the forwarding table for destination.
        * \param dest destination address
        * \param oif output interface if any (put 0 otherwise)
        * \return Ipv4Route to route the packet to reach dest address
        */
        Ptr<Ipv4Route> LookupHULARoute (Flowlet *flowlet, Ptr<NetDevice> oif = 0);

        int32_t GetFlowletOutPort(Flowlet *flowlet);

        Flowlet *GetFlowlet(uint32_t hash);

        void AddOrUpdateFlowlet(Flowlet *flowlet);

        void DeleteFlowlet(uint32_t hash);

        /**
        * \brief Delete a route.
        * \param route the route to be removed
        */
        void DeleteRoute (uint32_t index);

        /**
         * \brief Handle a packet reception.
         *
         * This function is called by lower layers.
         *
         * \param socket the socket the packet was received to.
         */
        void HandleRead (Ptr<Socket> socket);

        void ProcessPacket(Ptr<Packet> packet, HULAHeader header, uint32_t incoming_interface);

        Ptr<Socket> m_socket;                  //!< The socket bound to port 43333

        Ptr<Ipv4> m_ipv4;

        double m_tau;

        double m_flowlet_interval;

        /**
        * If a device is an edge switch, block probe replication. False by default
        */
        bool m_isEdge;

        /**
        * Use queue depth instead of link congestion estimation
        */
        bool m_useQueue;
    };

}

#endif /* HULA_H */

