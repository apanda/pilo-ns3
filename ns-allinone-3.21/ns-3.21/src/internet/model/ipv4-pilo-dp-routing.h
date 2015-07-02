/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * PILO code PILO code PILO code
 */

#ifndef IPV4_PILO_DP_ROUTING_H
#define IPV4_PILO_DP_ROUTING_H
#include <utility>
#include <stdint.h>
#include <unordered_map>
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-header.h"
#include "ns3/socket.h"
#include "ns3/ptr.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/pilo-socket-factory.h"
#include "ns3/pilo-socket.h"
#include "ns3/pilo-header.h"
#include <random>

namespace ns3 {
class Packet;
class NetDevice;
class Ipv4Interface;
class Ipv4Address;
class Ipv4Header;
class Ipv4RoutingTableEntry;
class Ipv4MulticastRoutingTableEntry;
class Node;

struct InterfaceStateMessage {
  uint32_t switch_id;
  uint64_t link_id;
  uint64_t event_id;
  bool state;
  uint32_t other_switch_id;
  bool is_host;
};

struct link_state {
  uint64_t event_id;
  bool state;
  
  link_state() {
    event_id = 0;
    state = true;
  }
  
  link_state(uint64_t event_id_, bool state_) {
    event_id = event_id_;
    state = state_;
  }
};


class Ipv4PiloDPRouting : public Ipv4RoutingProtocol
{
  typedef std::unordered_map<Ipv4Address, uint32_t, Ipv4AddressHash> 
            RoutingTable;
  typedef std::unordered_map<Ipv4Address, uint32_t, Ipv4AddressHash> 
            AddressToIface;
  typedef std::unordered_map<uint32_t, uint32_t> 
            IfaceToNode;
public:
  Ipv4PiloDPRouting ();
  ~Ipv4PiloDPRouting();

  static TypeId GetTypeId (void);

  /**
   * \brief Query routing cache for an existing route, for an outbound packet
   *
   * This lookup is used by transport protocols.  It does not cause any
   * packet to be forwarded, and is synchronous.  Can be used for
   * multicast or unicast.  The Linux equivalent is ip_route_output()
   *
   * The header input parameter may have an uninitialized value
   * for the source address, but the destination address should always be 
   * properly set by the caller.
   *
   * \param p packet to be routed.  Note that this method may modify the packet.
   *          Callers may also pass in a null pointer. 
   * \param header input parameter (used to form key to search for the route)
   * \param oif Output interface Netdevice.  May be zero, or may be bound via
   *            socket options to a particular output interface.
   * \param sockerr Output parameter; socket errno 
   *
   * \returns a code that indicates what happened in the lookup
   */
  virtual Ptr<Ipv4Route> RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr);

  /**
   * \brief Route an input packet (to be forwarded or locally delivered)
   *
   * This lookup is used in the forwarding process.  The packet is
   * handed over to the Ipv4RoutingProtocol, and will get forwarded onward
   * by one of the callbacks.  The Linux equivalent is ip_route_input().
   * There are four valid outcomes, and a matching callbacks to handle each.
   *
   * \param p received packet
   * \param header input parameter used to form a search key for a route
   * \param idev Pointer to ingress network device
   * \param ucb Callback for the case in which the packet is to be forwarded
   *            as unicast
   * \param mcb Callback for the case in which the packet is to be forwarded
   *            as multicast
   * \param lcb Callback for the case in which the packet is to be locally
   *            delivered
   * \param ecb Callback to call if there is an error in forwarding
   * \returns true if the Ipv4RoutingProtocol takes responsibility for 
   *          forwarding or delivering the packet, false otherwise
   */ 
  virtual bool RouteInput  (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev, 
                            UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                            LocalDeliverCallback lcb, ErrorCallback ecb);

  /**
   * \param interface the index of the interface we are being notified about
   *
   * Protocols are expected to implement this method to be notified of the state change of
   * an interface in a node.
   */
  virtual void NotifyInterfaceUp (uint32_t interface);
  /**
   * \param interface the index of the interface we are being notified about
   *
   * Protocols are expected to implement this method to be notified of the state change of
   * an interface in a node.
   */
  virtual void NotifyInterfaceDown (uint32_t interface);

  /**
   * \param interface the index of the interface we are being notified about
   * \param address a new address being added to an interface
   *
   * Protocols are expected to implement this method to be notified whenever
   * a new address is added to an interface. Typically used to add a 'network route' on an
   * interface. Can be invoked on an up or down interface.
   */
  virtual void NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address);

  /**
   * \param interface the index of the interface we are being notified about
   * \param address a new address being added to an interface
   *
   * Protocols are expected to implement this method to be notified whenever
   * a new address is removed from an interface. Typically used to remove the 'network route' of an
   * interface. Can be invoked on an up or down interface.
   */
  virtual void NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address);

  /**
   * \param ipv4 the ipv4 object this routing protocol is being associated with
   * 
   * Typically, invoked directly or indirectly from ns3::Ipv4::SetRoutingProtocol
   */
  virtual void SetIpv4 (Ptr<Ipv4> ipv4);

  /**
   * \brief Print the Routing Table entries
   *
   * \param stream the ostream the Routing table is printed to
   */
  virtual void PrintRoutingTable (Ptr<OutputStreamWrapper> stream) const;
  
  void SetSwitchId(uint32_t id);
  uint32_t GetSwitchId();
  uint64_t GetLinkId(uint32_t switch_id0, uint32_t switch_id1);
  uint32_t GetOtherSwitchId(uint64_t link_, uint32_t switch_id);
  void SendLinkStates(uint32_t target);

protected:
  void HandlePiloControlPacket (const PiloHeader& hdr, Ptr<Packet> pkt);
  void HandleRead (Ptr<Socket> socket);
  Ptr<Ipv4> m_ipv4; // IPv4 instance for this router
  RoutingTable m_routingTable;
  AddressToIface m_addressIface;
  IfaceToNode m_ifaceToNode;
  Ptr<PiloSocket> m_socket; //!< PILO socket
  static const uint16_t PORT = 6500;
  uint32_t switch_id;
  std::map<uint64_t, link_state> *link_states; // link_id: event_id
  std::set<uint32_t> *hosts; // a set of hosts that are currently connected to this switch

  std::set<uint64_t> id_seen;

  std::random_device rd;
  std::mt19937 gen;
  std::uniform_real_distribution<double> dis;
};

}
#endif
