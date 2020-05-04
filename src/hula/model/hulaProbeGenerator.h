/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* Author: Antonio Marsico <antonio.marsico@bt.com> */

#ifndef HULA_PROBE_GENERATOR_H
#define HULA_PROBE_GENERATOR_H

#include "ns3/application.h"
#include "ns3/inet-socket-address.h"
#include "ns3/ptr.h"
#include "hula_hdr.h"

namespace ns3 {

    class Socket;
    class Packet;

/* ... */
    class HULAProbeGenerator : public Application
    {
    public:
        /**
        * \brief Get the type ID.
        * \return the object TypeId
        */
        static TypeId GetTypeId (void);

        HULAProbeGenerator();
        ~HULAProbeGenerator() override;

    protected:
        void DoDispose(void) override;

    private:
        static const int PORT = 43333;

        void StartApplication(void) override;

        void StopApplication(void) override;

        void SendProbe();

        Ptr<Socket> m_socket;                  //!< The socket bound to port 43333

        Time m_delay;

        Ipv4Address m_ipv4Address;

        Ipv4Mask m_ipv4Mask;

        EventId m_probe_generation_event;
    };

}

#endif /* HULA_H */

