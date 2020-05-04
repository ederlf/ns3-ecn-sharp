/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* Author: Antonio Marsico <antonio.marsico@bt.com> */

#include "hula_hdr.h"
#include "ns3/packet.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

namespace ns3 {

    NS_LOG_COMPONENT_DEFINE ("HULAHeader");

    NS_OBJECT_ENSURE_REGISTERED (HULAHeader);

    TypeId
    HULAHeader::GetTypeId() {
        static TypeId tid = TypeId ("ns3::HULAHeader")
                .SetParent<Header> ()
                .SetGroupName ("HULA")
                .AddConstructor<HULAHeader> ()
        ;
        return tid;
    }

    HULAHeader::HULAHeader()
    : max_util_ (0),
      m_headerSize (24),
      timeStamp (0)
    {
        NS_LOG_FUNCTION(this);
    }

    HULAHeader::~HULAHeader() {
        NS_LOG_FUNCTION(this);
    }

    TypeId
    HULAHeader::GetInstanceTypeId () const
    {
        NS_LOG_FUNCTION (this);
        return GetTypeId ();
    }

    void
    HULAHeader::Serialize (Buffer::Iterator start) const
    {
        NS_LOG_FUNCTION (this << &start);
        Buffer::Iterator i = start;
        i.WriteHtonU32 (destination.Get());
        i.WriteHtonU32(subnetMask.Get());
        i.WriteU64(max_util_);
        i.WriteHtonU64(timeStamp);

    }

    uint32_t
    HULAHeader::Deserialize (Buffer::Iterator start)
    {
        NS_LOG_FUNCTION (this << &start);
        destination.Set(start.ReadNtohU32());
        subnetMask.Set(start.ReadNtohU32());
        max_util_ = start.ReadU64();
        timeStamp = start.ReadNtohU64();
        return GetSerializedSize();
    }

    uint32_t
    HULAHeader::GetSerializedSize (void) const
    {
        NS_LOG_FUNCTION (this);
        return m_headerSize;
    }


    void
    HULAHeader::Print (std::ostream &os) const
    {
        NS_LOG_FUNCTION (this << &os);
        os << "dst=" << destination << ", mask=" << subnetMask << ", max_util=" << max_util_ << ", timeStamp=" << timeStamp;
    }

    const Ipv4Address &HULAHeader::getDestination() const {
        return destination;
    }

    void HULAHeader::setDestination(const Ipv4Address &destination) {
        HULAHeader::destination = destination;
    }

    uint64_t HULAHeader::getMax_util_() const {
        return max_util_;
    }

    void HULAHeader::setMax_util_(uint64_t max_util_) {
        HULAHeader::max_util_ = max_util_;
    }

    uint64_t HULAHeader::GetTime() const {
        return timeStamp;
    }

    void HULAHeader::SetTime() {
        timeStamp = (uint64_t) Simulator::Now ().GetMicroSeconds();
    }

    void HULAHeader::SetTime(uint64_t timeStamp) {
        HULAHeader::timeStamp = timeStamp;
    }

    const Ipv4Mask &HULAHeader::getSubnetMask() const {
        return subnetMask;
    }

    void HULAHeader::setSubnetMask(const Ipv4Mask &subnetMask) {
        HULAHeader::subnetMask = subnetMask;
    }
}
