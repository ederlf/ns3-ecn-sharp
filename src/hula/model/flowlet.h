/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* Author: Antonio Marsico <antonio.marsico@bt.com> */

#ifndef FLOWLET_H
#define FLOWLET_H

#include "ns3/object.h"
#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"

namespace ns3 {
    class Flowlet : public Object {

    public:
        /**
        * \brief Get the type ID.
        * \return the object TypeId
        */
        static TypeId GetTypeId (void);

        Flowlet();

        /**
        * \brief Copy Constructor
        * \param route The route to copy
        */
        Flowlet (Flowlet const *flowlet);

        Flowlet(Ipv4Address src,
                Ipv4Address dst,
                uint8_t proto,
                uint32_t src_port,
                uint32_t dst_port);

        virtual ~Flowlet();

        const Ipv4Address &getM_src_address() const;

        const Ipv4Address &getM_dst_address() const;

        uint8_t getProtocol() const;

        const Time &getLastSeenTime() const;

        void setLastSeenTime(const Time &lastSeenTime);

		uint32_t getLast_flow_size() const;

		void set_last_flow_size(uint32_t last_flow_size);

		uint32_t get_current_flow_size() const;

		void set_current_flow_size(uint32_t last_flow_size);

        int32_t getLast_out_interface() const;

        void setLast_out_interface(int32_t last_out_interface);

        uint32_t getHash() const;

        static uint32_t CalculateFlowletHash(Ipv4Address src,
                               Ipv4Address dst,
                               uint8_t proto,
                               uint32_t src_port,
                               uint32_t dst_port);

    private:
        Ipv4Address m_src_address;
        Ipv4Address m_dst_address;
        uint8_t protocol;
        uint32_t m_src_port;
        uint32_t m_dst_port;
        Time lastSeenTime;
		uint32_t last_flow_size;
		uint32_t current_flow_size;
        int32_t last_out_interface;
        uint32_t hash;
    };
}


#endif //NS_3_28_FLOWLET_H
