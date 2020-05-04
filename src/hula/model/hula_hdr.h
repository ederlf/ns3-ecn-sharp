/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* Author: Antonio Marsico <antonio.marsico@bt.com> */

#ifndef HULA_HDR_H
#define HULA_HDR_H

#include "ns3/header.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-header.h"
#include <stdint.h>

namespace ns3 {

    class Packet;

    class HULAHeader: public Header {
    public:

        /**
       * \brief Get the type ID.
       * \return the object TypeId
       */
        static TypeId GetTypeId(void);

        HULAHeader();
        virtual ~HULAHeader();

        const Ipv4Address &getDestination() const;

        void setDestination(const Ipv4Address &destination);

        uint64_t getMax_util_() const;

        void setMax_util_(uint64_t max_util_);

        /**
        * \brief Set the time when message is sent
        */
        void SetTime ();

        void SetTime (uint64_t timeStamp);

        uint64_t GetTime () const;

        const Ipv4Mask &getSubnetMask() const;

        void setSubnetMask(const Ipv4Mask &subnetMask);

        virtual TypeId GetInstanceTypeId (void) const;
        virtual uint32_t GetSerializedSize (void) const;
        virtual void Serialize (Buffer::Iterator start) const;
        virtual uint32_t Deserialize (Buffer::Iterator start);
        virtual void Print (std::ostream &os) const;

    private:
        Ipv4Address destination; //!< Host destination
        Ipv4Mask subnetMask;
        uint64_t max_util_; //!< Max link utilization
        uint16_t m_headerSize;
        uint64_t timeStamp;  //!< true if downstream

    };
}

#endif
