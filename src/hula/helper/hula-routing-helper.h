/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* Author: Antonio Marsico <antonio.marsico@bt.com> */

#ifndef HULA_ROUTING_HELPER_H
#define HULA_ROUTING_HELPER_H

#include "ns3/object-factory.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/node-container.h"
#include "ns3/node.h"

namespace ns3 {

    class HULARoutingHelper : public Ipv4RoutingHelper {
    public:
        HULARoutingHelper();

        HULARoutingHelper (const HULARoutingHelper &o);

        ~HULARoutingHelper() override;

        HULARoutingHelper *Copy(void) const override;

        Ptr<Ipv4RoutingProtocol> Create(Ptr<Node> node) const override;

        /**
        * \param name the name of the attribute to set
        * \param value the value of the attribute to set.
        *
        * This method controls the attributes of ns3::Ripng
        */
        void Set (std::string name, const AttributeValue &value);

    private:
        /**
         * \brief Assignment operator declared private and not implemented to disallow
         * assignment and prevent the compiler from happily inserting its own.
         *
         * \param o The object to copy from.
         * \returns pointer to clone of this RipNgHelper
         */
        HULARoutingHelper &operator = (const HULARoutingHelper &o);

        ObjectFactory m_factory; //!< Object Factory
    };

}

#endif