/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* Author: Antonio Marsico <antonio.marsico@bt.com> */

#include "flowlet.h"
#include "ns3/log.h"
#include "ns3/hash.h"
#include "ns3/simulator.h"
#include <sstream>
#include <string>

namespace ns3 {
    NS_LOG_COMPONENT_DEFINE ("Flowlet");

    NS_OBJECT_ENSURE_REGISTERED (Flowlet);

    TypeId
    Flowlet::GetTypeId (void) {
        static TypeId tid = TypeId("ns3::Flowlet")
                .SetParent<Object>()
                .SetGroupName("Internet");

        return tid;
    }

    Flowlet::Flowlet() {
        NS_LOG_FUNCTION (this);
        m_src_port = 0;
        m_dst_port = 0;
        protocol = 0;
		last_flow_size = 0;
		current_flow_size = 0;
        last_out_interface = 0;
    }

    /** Constructor **/
    Flowlet::Flowlet(Ipv4Address src, Ipv4Address dst, uint8_t proto, uint32_t src_port, uint32_t dst_port) {

        m_src_address = src;
        m_dst_address = dst;
        protocol = proto;
        m_src_port = src_port;
        m_dst_port = dst_port;
        last_out_interface = 0;
        lastSeenTime = Simulator::Now ();
		last_flow_size = 0;
		current_flow_size = 0;
        hash = CalculateFlowletHash(m_src_address, m_dst_address, protocol, m_src_port, m_dst_port);
    }

    Flowlet::~Flowlet() {
        NS_LOG_FUNCTION (this);
    }

    const Ipv4Address &Flowlet::getM_src_address() const {
        return m_src_address;
    }

    const Ipv4Address &Flowlet::getM_dst_address() const {
        return m_dst_address;
    }

    uint8_t Flowlet::getProtocol() const {
        return protocol;
    }

    const Time &Flowlet::getLastSeenTime() const {
        return lastSeenTime;
    }

    void Flowlet::setLastSeenTime(const Time &lastSeenTime) {
        Flowlet::lastSeenTime = lastSeenTime;
    }

	uint32_t Flowlet::getLast_flow_size() const
	{
		return last_flow_size;
	}

	void Flowlet::set_last_flow_size(uint32_t last_flow_size)
	{
		Flowlet::last_flow_size = last_flow_size;
	}

	uint32_t Flowlet::get_current_flow_size() const
	{
		return current_flow_size;
	}

	void Flowlet::set_current_flow_size(uint32_t last_flow_size)
	{
		Flowlet::current_flow_size = last_flow_size;
	}

    int32_t Flowlet::getLast_out_interface() const {
        return last_out_interface;
    }

    void Flowlet::setLast_out_interface(int32_t last_out_interface) {
        Flowlet::last_out_interface = last_out_interface;
    }

    uint32_t Flowlet::getHash() const {
        return hash;
    }

    /** Calculates the hash of the flowlet **/
    uint32_t Flowlet::CalculateFlowletHash(Ipv4Address src,
            Ipv4Address dst, uint8_t proto, uint32_t src_port,
                                           uint32_t dst_port) {

        std::stringstream ss;
        ss << src << " " << dst << " "
           << proto << " " << src_port << " " << dst_port;
        std::string hashString = ss.str();

        return Hash32(hashString);
    }

    /** Copy Constructor **/
    Flowlet::Flowlet(const Flowlet *flowlet)
    : m_src_address(flowlet->m_src_address),
    m_dst_address(flowlet->m_dst_address),
    protocol(flowlet->protocol),
    m_src_port(flowlet->m_src_port),
    m_dst_port(flowlet->m_dst_port),
    lastSeenTime(flowlet->lastSeenTime),
	last_flow_size(flowlet->last_flow_size),
	current_flow_size(flowlet->current_flow_size),
    last_out_interface(flowlet->last_out_interface),
    hash(flowlet->hash)
    {
        NS_LOG_FUNCTION (this << flowlet);
    }
}
