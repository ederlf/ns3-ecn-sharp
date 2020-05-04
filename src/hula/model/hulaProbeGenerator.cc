/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* Author: Antonio Marsico <antonio.marsico@bt.com> */

#include "hulaProbeGenerator.h"
#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/socket-factory.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/net-device.h"
#include "ns3/ipv4-packet-info-tag.h"
#include "ns3/ipv4.h"

namespace ns3 {

    NS_LOG_COMPONENT_DEFINE ("HULAProbeGenerator");
    NS_OBJECT_ENSURE_REGISTERED (HULAProbeGenerator);

    /**
     * \ingroup hula
     *
     * \class HULAProbeGenerator
     * \brief Proccess the HULA packets and updates the routing table accordingly
     */

    TypeId HULAProbeGenerator::GetTypeId(void) {
        static TypeId tid = TypeId ("ns3::HULAProbeGenerator")
                .SetParent<Application> ()
                .AddConstructor<HULAProbeGenerator>()
                .SetGroupName ("HULA")
                .AddAttribute ("Interval", "Time for retransmission of HULA Probe",
                               TimeValue (MicroSeconds(200)),
                               MakeTimeAccessor (&HULAProbeGenerator::m_delay),
                               MakeTimeChecker ())
                .AddAttribute ("Address", "Address to advertise",
                               Ipv4AddressValue(),
                               MakeIpv4AddressAccessor (&HULAProbeGenerator::m_ipv4Address),
                               MakeIpv4AddressChecker())
                .AddAttribute ("Mask", "Time for retransmission of HULA Probe",
                               Ipv4MaskValue(),
                               MakeIpv4MaskAccessor (&HULAProbeGenerator::m_ipv4Mask),
                               MakeIpv4MaskChecker())
        ;
        return tid;
    }

    HULAProbeGenerator::HULAProbeGenerator() {

    }

    HULAProbeGenerator::~HULAProbeGenerator() {

    }

    void HULAProbeGenerator::DoDispose(void) {
        NS_LOG_FUNCTION (this);
        Application::DoDispose();
    }

    void HULAProbeGenerator::StartApplication(void) {

        NS_LOG_FUNCTION (this);

        if (m_socket == nullptr) {

            Ptr<Ipv4> ipv4 = GetNode ()->GetObject<Ipv4> ();

            TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
            m_socket = Socket::CreateSocket (GetNode (), tid);
            InetSocketAddress local = InetSocketAddress (ipv4->GetAddress (1, 0).GetLocal());
            m_socket->SetAllowBroadcast (true);
            m_socket->BindToNetDevice(ipv4->GetNetDevice(1));
            m_socket->Bind (local);
            //m_socket->SetRecvPktInfo (true);
        }

        SendProbe();

    }

    void HULAProbeGenerator::StopApplication(void) {

        NS_LOG_FUNCTION (this);
        m_probe_generation_event.Cancel();

        if (m_socket != nullptr)
        {
            m_socket->Close ();
            m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
            m_socket = nullptr;
        }
    }

    void HULAProbeGenerator::SendProbe() {
        NS_LOG_FUNCTION (this);

        HULAHeader header;
        Ptr<Packet> packet;
        packet = Create<Packet> ();

        Ptr<Ipv4> ipv4 = GetNode ()->GetObject<Ipv4> ();

        if (m_ipv4Address == Ipv4Address()) {
            Ipv4InterfaceAddress interfaceAddress = ipv4->GetAddress(1,0);
            header.setDestination(interfaceAddress.GetLocal());
            header.setSubnetMask(interfaceAddress.GetMask());
        } else {

            header.setDestination(m_ipv4Address);
            header.setSubnetMask(m_ipv4Mask);
        }


        header.setMax_util_(0);
        header.SetTime();

        packet->AddHeader(header);

        NS_LOG_INFO ("SendTo: " << *packet);

        if ((m_socket->SendTo (packet, 0, InetSocketAddress (Ipv4Address ("255.255.255.255"), PORT))) >= 0)
        {
            NS_LOG_INFO ("HULA Probe sent" );
        }
        else
        {
            NS_LOG_INFO ("Error while sending HULA Probe");
        }

        m_probe_generation_event = Simulator::Schedule (m_delay, &HULAProbeGenerator::SendProbe, this);
    }
}

