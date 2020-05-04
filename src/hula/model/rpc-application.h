/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* Author: Antonio Marsico <antonio.marsico@bt.com> */

#ifndef RPC_APPLICATION_H
#define RPC_APPLICATION_H

#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/data-rate.h"
#include "ns3/traced-callback.h"
#include "ns3/random-variable-stream.h"
#include <queue>


namespace ns3 {

class Address;
class RandomVariableStream;
class Socket;

struct FlowStat {
    Time flow_start_time;
    uint64_t flow_size;
    bool completed;

    FlowStat()
    : flow_start_time(0),
    flow_size(0),
    completed(false)
    {}
};

/**
 * \ingroup applications 
 * \defgroup hula RPCApplication
 *
 * This traffic generator follows a Poisson process to generate traffic.
 */
/**
* \ingroup hula
*
* \brief Generate traffic to a single destination according to a Poisson procees
*        and a CDF that contains a traffic pattern.
*/
class RPCApplication : public Application
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  RPCApplication ();

  virtual ~RPCApplication();

  /**
   * \brief Return a pointer to associated socket.
   * \return pointer to associated socket
   */
  Ptr<Socket> GetSocket (void) const;

  double GetTotalFCT();

  double GetTotalLongFCT();

  double GetTotalShortFCT();

  uint32_t GetTotalConnections();

  uint32_t GetTotalLongConnections();

  uint32_t GetTotalShortConnections();

 /**
  * \brief Assign a fixed random variable stream number to the random variables
  * used by this model.
  *
  * \param stream first stream index to use
  * \return the number of stream indices assigned by this model
  */
  int64_t AssignStreams (int64_t stream);

protected:
  virtual void DoDispose (void);
private:
  // inherited from Application base class.
  virtual void StartApplication (void);    // Called at time specified by Start
  virtual void StopApplication (void);     // Called at time specified by Stop

  //helpers
  /**
   * \brief Cancel all pending events.
   */
  void CancelEvents ();

  void SendData (uint32_t tx_available=0);

  Ptr<Socket>     m_socket;       //!< Associated socket
  Address         m_peer;         //!< Peer address
  bool            m_connected;    //!< True if connected
  bool            m_transmitting;
  Ptr<ExponentialRandomVariable>  m_exp_random;       //!< rng for On Time
  Ptr<RandomVariableStream>  m_constant_random;      //!< rng for Off Time
  uint32_t        m_pktSize;      //!< Size of packets
  uint32_t        m_residualBits; //!< Number of generated, but not sent, bits
  Time            m_lastStartTime; //!< Time last packet sent
  uint32_t        m_maxConnections;
  double          m_flowRate;
  uint32_t        m_totConnections;     //!< Total connections sent so far
  uint32_t        m_generatedConnections;     //!< Total connections sent so far
  EventId         m_sendEvent;    //!< Event id of pending "send packet" event
  TypeId          m_tid;          //!< Type of the socket used
  uint64_t        m_maxBytesPerConnection;
  uint64_t        m_totBytesPerConnection;
  double          m_average_fct;
  double          m_average_fct_long;
  uint32_t        m_num_connections_long;
  double          m_average_fct_short;
  uint32_t        m_num_connections_short;
  uint32_t        m_socket_buffer_size;

  FlowStat        m_current_flow;

  std::vector<uint32_t> m_flow_sizes;
  std::string m_cdf_file;
  std::queue<FlowStat> m_flow_queue;

  /// Traced Callback: transmitted packets.
  TracedCallback<Ptr<const Packet> > m_txTrace;

  /**
   * \brief Schedule the next packet transmission
   */
  void ScheduleNextTx ();
  /**
   * \brief Handle a Connection Succeed event
   * \param socket the connected socket
   */
  void ConnectionSucceeded (Ptr<Socket> socket);
  /**
   * \brief Handle a Connection Failed event
   * \param socket the not connected socket
   */
  void ConnectionFailed (Ptr<Socket> socket);

  void DataSend (Ptr<Socket>, uint32_t tx_available);
};

} // namespace ns3

#endif /* ONOFF_APPLICATION_H */
