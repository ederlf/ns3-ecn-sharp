/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* Author: Antonio Marsico <antonio.marsico@bt.com> */

#include "hula-generator-helper.h"
#include "ns3/log.h"
#include "ns3/hulaProbeGenerator.h"

namespace ns3 {
    NS_LOG_COMPONENT_DEFINE ("HULAHelper");

/* ... */

    HulaGeneratorHelper::HulaGeneratorHelper() {
        m_generatorFactory.SetTypeId (HULAProbeGenerator::GetTypeId ());
    }

    void
    HulaGeneratorHelper::SetGeneratorAttribute (
            std::string name,
            const AttributeValue &value)
    {
        m_generatorFactory.Set (name, value);
    }

    ApplicationContainer HulaGeneratorHelper::Install(NodeContainer c) const {
        ApplicationContainer apps;
        for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
        {
            apps.Add (InstallGeneratorPriv (*i));
        }

        return apps;
    }

    ApplicationContainer HulaGeneratorHelper::Install(Ptr<Node> node) const {
        return ApplicationContainer(InstallGeneratorPriv(node));
    }

    Ptr<Application> HulaGeneratorHelper::InstallGeneratorPriv(Ptr<Node> node) const {

        Ptr<Application> app = m_generatorFactory.Create<HULAProbeGenerator> ();
        node->AddApplication (app);

        return app;
    }
}

