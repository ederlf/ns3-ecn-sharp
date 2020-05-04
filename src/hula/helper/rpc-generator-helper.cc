/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* Author: Antonio Marsico <antonio.marsico@bt.com> */

#include "rpc-generator-helper.h"
#include "ns3/inet-socket-address.h"
#include "ns3/packet-socket-address.h"
#include "ns3/string.h"
#include "ns3/names.h"
#include "ns3/rpc-application.h"

namespace ns3 {

RPCGeneratorHelper::RPCGeneratorHelper (std::string protocol, Address address)
{
  m_factory.SetTypeId ("ns3::RPCApplication");
  m_factory.Set ("Protocol", StringValue (protocol));
  m_factory.Set ("Remote", AddressValue (address));
}

void
RPCGeneratorHelper::SetAttribute (std::string name, const AttributeValue &value)
{
  m_factory.Set (name, value);
}

ApplicationContainer
RPCGeneratorHelper::Install (Ptr<Node> node) const
{
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
RPCGeneratorHelper::Install (std::string nodeName) const
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
RPCGeneratorHelper::Install (NodeContainer c) const
{
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      apps.Add (InstallPriv (*i));
    }

  return apps;
}

int64_t RPCGeneratorHelper::AssignStreams(NodeContainer c, int64_t stream)
{
	int64_t currentStream = stream;
	Ptr<Node> node;
	for (NodeContainer::Iterator i = c.Begin(); i != c.End(); ++i)
	{
		node = (*i);
		for (uint32_t j = 0; j < node->GetNApplications(); j++)
		{
			Ptr<RPCApplication> rpcapp = DynamicCast<RPCApplication>(node->GetApplication(j));
			if (rpcapp)
			{
				currentStream += rpcapp->AssignStreams(currentStream);
			}
		}
	}
	return (currentStream - stream);
}

Ptr<Application>
RPCGeneratorHelper::InstallPriv (Ptr<Node> node) const
{
  Ptr<Application> app = m_factory.Create<Application> ();
  node->AddApplication (app);

  return app;
}

} // namespace ns3
