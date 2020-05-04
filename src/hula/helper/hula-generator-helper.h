/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* Author: Antonio Marsico <antonio.marsico@bt.com> */

#ifndef HULA_GENERATOR_HELPER_H
#define HULA_GENERATOR_HELPER_H

#include "ns3/object-factory.h"
#include "ns3/attribute.h"
#include "ns3/net-device.h"
#include "ns3/node-container.h"
#include "ns3/application-container.h"

namespace ns3 {

/* ... */

class HulaGeneratorHelper
{
public:
    HulaGeneratorHelper();

    /**
     * Record an attribute to be set in each Application after it is is created.
     *
     * \param name the name of the attribute to set
     * \param value the value of the attribute to set
     */
    void SetGeneratorAttribute (std::string name, const AttributeValue &value);

    /**
    * Install an ns3::HULAProbeGenerator on each node of the input container
    * configured with all the attributes set with SetAttribute.
    *
    * \param c NodeContainer of the set of nodes on which a HULA probe generator
    * will be installed.
    * \returns Container of Ptr to the applications installed.
    */
    ApplicationContainer Install (NodeContainer c) const;

    /**
     * Install an ns3::HULAProbeGenerator on the node configured with all the
     * attributes set with SetAttribute.
     *
     * \param node The node on which an OnOffApplication will be installed.
     * \returns Container of Ptr to the applications installed.
     */
    ApplicationContainer Install(Ptr<Node> node) const;

private:

    /**
    * Install an ns3::HULAGenerator on the node configured with all the
    * attributes set with SetAttribute.
    *
    * \param node The node on which an HULAGenerator will be installed.
    * \returns Ptr to the application installed.
    */
    Ptr<Application> InstallGeneratorPriv (Ptr<Node> node) const;

    ObjectFactory m_generatorFactory;                 //!< HULA generator factory
};

}

#endif /* HULA_HELPER_H */

