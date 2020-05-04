/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* Author: Antonio Marsico <antonio.marsico@bt.com> */

// ns3 - RPC Application class
// Adapted from ApplicationOnOff in GTNetS.

#include "ns3/log.h"
#include "ns3/address.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/packet-socket-address.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "rpc-application.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/string.h"
#include "ns3/pointer.h"
#include "ns3/double.h"
#include <fstream>
#include <iostream>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("RPCApplication");

NS_OBJECT_ENSURE_REGISTERED (RPCApplication);

TypeId
RPCApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::RPCApplication")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<RPCApplication> ()
    .AddAttribute ("PacketSize", "The size of packets sent in on state",
                   UintegerValue (1460),
                   MakeUintegerAccessor (&RPCApplication::m_pktSize),
                   MakeUintegerChecker<uint32_t> (1))
    .AddAttribute ("MaxConnections", "The maximum number of connections to generate",
                   UintegerValue (20),
                   MakeUintegerAccessor (&RPCApplication::m_maxConnections),
                   MakeUintegerChecker<uint32_t> (1))
    .AddAttribute ("FlowRate", "Number of flows per seconds to generate",
                   DoubleValue (5.0),
                   MakeDoubleAccessor (&RPCApplication::m_flowRate),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("FilePath", "Path to the CDF file",
                   StringValue ("./src/hula/examples/cdf.txt"),
                   MakeStringAccessor (&RPCApplication::m_cdf_file),
                   MakeStringChecker())
    .AddAttribute ("Remote", "The address of the destination",
                   AddressValue (),
                   MakeAddressAccessor (&RPCApplication::m_peer),
                   MakeAddressChecker ())
    .AddAttribute ("Protocol", "The type of protocol to use. This should be "
                   "a subclass of ns3::SocketFactory",
                   TypeIdValue (UdpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&RPCApplication::m_tid),
                   // This should check for SocketFactory as a parent
                   MakeTypeIdChecker ())
    .AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&RPCApplication::m_txTrace),
                     "ns3::Packet::TracedCallback")
  ;
  return tid;
}


RPCApplication::RPCApplication ()
  : m_socket (0),
    m_connected (false),
    m_transmitting(false),
	m_exp_random(CreateObject<ExponentialRandomVariable>()),
    m_constant_random(CreateObject<UniformRandomVariable>()),
    m_residualBits (0),
    m_lastStartTime (Seconds (0)),
    m_totConnections (0),
    m_generatedConnections(0),
    m_maxBytesPerConnection(0),
    m_totBytesPerConnection(0),
    m_average_fct(0.0),
    m_average_fct_long(0.0),
    m_num_connections_long(0),
    m_average_fct_short(0.0),
    m_num_connections_short(0),
    m_socket_buffer_size(0)
{
  NS_LOG_FUNCTION (this);
}

RPCApplication::~RPCApplication()
{
  NS_LOG_FUNCTION (this);
}

Ptr<Socket>
RPCApplication::GetSocket (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socket;
}

int64_t 
RPCApplication::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  m_exp_random->SetStream (stream);
  m_constant_random->SetStream (stream + 1);
  return 2;
}

void
RPCApplication::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  m_socket = 0;
  // chain up
  Application::DoDispose ();
}

// Application Methods
void RPCApplication::StartApplication () // Called at time specified by Start
{
    NS_LOG_FUNCTION (this << Simulator::Now());

    // Create the socket if not already
    if (!m_socket)
    {
        m_socket = Socket::CreateSocket (GetNode (), m_tid);
        //Register the available buffer size
        m_socket_buffer_size = m_socket->GetTxAvailable();


        // Fatal error if socket type is not NS3_SOCK_STREAM or NS3_SOCK_SEQPACKET
        if (m_socket->GetSocketType () != Socket::NS3_SOCK_STREAM &&
            m_socket->GetSocketType () != Socket::NS3_SOCK_SEQPACKET)
        {
            NS_FATAL_ERROR ("Using BulkSend with an incompatible socket type. "
                            "BulkSend requires SOCK_STREAM or SOCK_SEQPACKET. "
                            "In other words, use TCP instead of UDP.");
        }

        if (Inet6SocketAddress::IsMatchingType (m_peer))
        {
            if (m_socket->Bind6 () == -1)
            {
                NS_FATAL_ERROR ("Failed to bind socket");
            }
        }
        else if (InetSocketAddress::IsMatchingType (m_peer))
        {
            if (m_socket->Bind () == -1)
            {
                NS_FATAL_ERROR ("Failed to bind socket");
            }
        }

        m_socket->Connect (m_peer);
        m_socket->ShutdownRecv ();
        m_socket->SetConnectCallback (
                MakeCallback (&RPCApplication::ConnectionSucceeded, this),
                MakeCallback (&RPCApplication::ConnectionFailed, this));
        m_socket->SetSendCallback (
                MakeCallback (&RPCApplication::DataSend, this));
    }

    std::ifstream in_file;
    in_file.open(m_cdf_file, std::ifstream::in);
    uint32_t flow_size;

    NS_ASSERT(in_file.good());
    while (in_file >> flow_size) {

        m_flow_sizes.push_back(flow_size);
    }

    in_file.close();


    m_exp_random->SetAttribute ("Mean", DoubleValue (1 / m_flowRate));
    m_constant_random->SetAttribute("Max", DoubleValue(m_flow_sizes.size()-1));
}

void RPCApplication::StopApplication () // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);

  CancelEvents ();
  if(m_socket != 0)
    {
      m_socket->Close ();
      m_connected = false;
    }
  else
    {
      NS_LOG_WARN ("RPCApplication found null socket to close in StopApplication");
    }
}

void RPCApplication::CancelEvents ()
{
  NS_LOG_FUNCTION (this);
  Simulator::Cancel (m_sendEvent);
}

void RPCApplication::SendData (uint32_t tx_available)
{
    NS_LOG_FUNCTION (this);

    while (m_totBytesPerConnection < m_maxBytesPerConnection && m_totConnections < m_maxConnections)
    { // Time to send more

        m_transmitting = true;

        // uint64_t to allow the comparison later.
        // the result is in a uint32_t range anyway, because
        // m_sendSize is uint32_t.
        uint64_t toSend = m_pktSize;
        // Make sure we don't send too many packets
        if (m_maxBytesPerConnection > 0)
        {
            toSend = std::min (toSend, m_maxBytesPerConnection - m_totBytesPerConnection);
        }


        //NS_LOG_INFO ("sending packet at " << Simulator::Now () << " Residual size " << m_maxBytesPerConnection - m_totBytesPerConnection);
        Ptr<Packet> packet = Create<Packet> (toSend);
        int actual = m_socket->Send (packet);
        if (actual > 0)
        {
            m_totBytesPerConnection += actual;
            m_txTrace (packet);
        }
        // We exit this loop when actual < toSend as the send side
        // buffer is full. The "DataSent" callback will pop when
        // some buffer space has freed ip.
        if ((unsigned)actual != toSend)
        {
            break;
        }
    }
    // Check if time to schedule a new flow (all packets sent)
    if (tx_available >= m_socket_buffer_size-m_pktSize) {
        NS_LOG_DEBUG("Socket buffer empty " << Simulator::Now());

        if (m_totBytesPerConnection == m_maxBytesPerConnection && m_connected && !m_current_flow.completed) {
            Time delta(Simulator::Now() - m_current_flow.flow_start_time);
            m_current_flow.completed = true;
            m_totConnections++;
            NS_LOG_DEBUG("Flow completed [Node: " << GetNode ()->GetId() << " length: " << m_current_flow.flow_size << ", FCT time: " << delta << ", Current connections: " << m_totConnections << "]");
            
            m_average_fct += delta.GetSeconds();

            //Based on the packet size 1500
            if (m_current_flow.flow_size <= 68) {
                m_num_connections_short++;
                m_average_fct_short += delta.GetSeconds();
            }

            if (m_current_flow.flow_size >= 6991) {
                m_num_connections_long++;
                m_average_fct_long += delta.GetSeconds();
            }


            if (m_totConnections < m_maxConnections) {
                if (m_flow_queue.empty()) {
                    m_transmitting = false;
                } else {
                    m_current_flow = m_flow_queue.front();
                    m_maxBytesPerConnection = m_current_flow.flow_size * m_pktSize;
                    m_flow_queue.pop();
                    m_totBytesPerConnection = 0;
                    Simulator::ScheduleNow(&RPCApplication::SendData, this, tx_available);

                }
            }
        }
    }
}

// Private helpers
void RPCApplication::ScheduleNextTx ()
{
  NS_LOG_FUNCTION (this);

  if (m_maxConnections == 0 || m_generatedConnections < m_maxConnections)
    {

        FlowStat current_flow;
        current_flow.flow_start_time = Simulator::Now();
        current_flow.flow_size = m_flow_sizes.at(m_constant_random->GetInteger());
        current_flow.completed = false;

        NS_LOG_INFO("Next flow size " << current_flow.flow_size);
        NS_LOG_INFO("Next flow start time " << current_flow.flow_start_time);

        if (m_flow_queue.empty() && !m_transmitting) {
          //Schedule now send data
          //m_transmitting = true;
          m_current_flow = current_flow;
          m_totBytesPerConnection = 0;
          m_maxBytesPerConnection = m_current_flow.flow_size * m_pktSize;
          Simulator::ScheduleNow(&RPCApplication::SendData, this, 0);
        } else {
            m_flow_queue.push(current_flow);
        }

        m_generatedConnections++;

        Time nextTime (Seconds (m_exp_random->GetValue ())); // Time till next flow
        NS_LOG_LOGIC ("nextTime = " << nextTime);
        m_sendEvent = Simulator::Schedule (nextTime,
                                         &RPCApplication::ScheduleNextTx, this);
    }
  //else
    //{ // All done, cancel any pending events
      //StopApplication ();
    //}
}


void RPCApplication::ConnectionSucceeded (Ptr<Socket> socket)
{
    NS_LOG_FUNCTION (this << socket);
    NS_LOG_INFO("Socket connected");
    m_connected = true;
    double timeout = m_exp_random->GetValue ();
    m_sendEvent = Simulator::Schedule (Seconds (timeout), &RPCApplication::ScheduleNextTx, this);
}

void RPCApplication::ConnectionFailed (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
}

void RPCApplication::DataSend (Ptr<Socket>, uint32_t tx_available)
{
    NS_LOG_FUNCTION (this);
    if (m_connected && !m_current_flow.completed && m_current_flow.flow_size != 0)
    { // Only send new data if the connection has completed
        SendData (tx_available);
    }
}

double RPCApplication::GetTotalFCT()
{
    NS_LOG_FUNCTION(this << m_average_fct);
    return m_average_fct;
}

double RPCApplication::GetTotalLongFCT()
{
    NS_LOG_FUNCTION(this << m_average_fct_long);
    return m_average_fct_long;
}

double RPCApplication::GetTotalShortFCT()
{
    NS_LOG_FUNCTION(this << m_average_fct_short);
    return m_average_fct_short;
}

uint32_t RPCApplication::GetTotalConnections()
{
    return m_totConnections;
}

uint32_t RPCApplication::GetTotalShortConnections()
{
    return m_num_connections_short;
}

uint32_t RPCApplication::GetTotalLongConnections()
{
    return m_num_connections_long;
}


} // Namespace ns3
