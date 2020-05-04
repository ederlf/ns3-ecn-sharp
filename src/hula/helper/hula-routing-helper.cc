/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* Author: Antonio Marsico <antonio.marsico@bt.com> */

#include "hula-routing-helper.h"
#include "ns3/node.h"
#include "ns3/node-list.h"
#include "ns3/ipv4-list-routing.h"
#include "ns3/hula-routing.h"

namespace ns3 {

    HULARoutingHelper::HULARoutingHelper() {
        m_factory.SetTypeId ("ns3::HULARouting");
    }

    HULARoutingHelper::HULARoutingHelper (const HULARoutingHelper &o)
            : m_factory (o.m_factory) {}

    HULARoutingHelper::~HULARoutingHelper() {

    }

    HULARoutingHelper *HULARoutingHelper::Copy(void) const {
        return new HULARoutingHelper (*this);
    }

    Ptr<Ipv4RoutingProtocol> HULARoutingHelper::Create(Ptr<Node> node)

    const {

        Ptr<HULARouting> hula_routing = m_factory.Create<HULARouting> ();

        node->AggregateObject(hula_routing);

        return hula_routing;
    }

    void HULARoutingHelper::Set(std::string name, const AttributeValue &value) {
        m_factory.Set (name, value);
    }
}
