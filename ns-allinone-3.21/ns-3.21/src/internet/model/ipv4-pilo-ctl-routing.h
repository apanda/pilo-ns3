/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * PILO code PILO code PILO code
 */
#ifndef IPV4_PILO_CTL_ROUTING_H
#define IPV4_PILO_CTL_ROUTING_H

#include <list>
#include <utility>
#include <stdint.h>
#include <unordered_map>
#include <unordered_set>
#include <cinttypes>
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-header.h"
#include "ns3/socket.h"
#include "ns3/ptr.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/simulator.h"
#include "ns3/channel.h"
#include "ns3/ipv4-pilo-dp-routing-helper.h"

namespace ns3 {
class Packet;
class NetDevice;
class Ipv4Interface;
class Ipv4Address;
class Ipv4Header;
class Ipv4RoutingTableEntry;
class Ipv4MulticastRoutingTableEntry;
class Node;


class Ipv4PiloCtlRouting : public Ipv4RoutingProtocol
{
  typedef std::unordered_map<uint32_t, std::unordered_set<uint16_t> > 
            previous_packets;
public:
  Ipv4PiloCtlRouting ();
  ~Ipv4PiloCtlRouting ();

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

public:
  void GetBandwidthInfo() {
    std::map<uint64_t, uint64_t>::iterator it = bandwidth_per_link->begin();  
    std::map<uint64_t, uint64_t>::iterator it_end = bandwidth_per_link->end();

    Ipv4PiloDPRoutingHelper dpRouting;
    Ptr<Ipv4PiloDPRouting> dp_node = dpRouting.GetPiloDPRouting(m_ipv4);

    printf("[bw]Bandwidth info for from switch %u\n", dp_node->GetSwitchId());

    for (; it != it_end; it++) {
      uint64_t link_id = it->first;
      uint64_t bytes = it->second;

      uint32_t switch0 = GetSwitch0(link_id);
      uint32_t switch1 = GetSwitch1(link_id);

      double bw = (double) (bytes) * 8 / 1000.0 / (Simulator::Now().GetSeconds());
      printf("[bw]Link id: %" PRIu64 ", %u-%u -- bandwidth: %f Mb/s\n", link_id, switch0, switch1, bw);
      
    }
    printf("[bw]Bandwidth info done for switch %u\n", dp_node->GetSwitchId());
  }

  uint32_t GetSwitch0(uint64_t link_id) {
    uint64_t link_ = link_id;
    uint32_t *ptr0 = (uint32_t *) &link_;
    return *ptr0;
  }

  uint32_t GetSwitch1(uint64_t link_id) {
    uint64_t link_ = link_id;
    uint32_t *ptr1 = ((uint32_t *) &link_)+1;
    return *ptr1;
  }

  uint64_t GetLinkId(uint32_t switch_id0, uint32_t switch_id1) {

    uint64_t link_id = 0;

    uint32_t *ptr0 = (uint32_t *) &link_id;
    uint32_t *ptr1 = ((uint32_t *) &link_id)+1;
        
    *ptr0 = switch_id0 < switch_id1 ? switch_id0 : switch_id1;
    *ptr1 = switch_id0 > switch_id1 ? switch_id0 : switch_id1;
    
    return link_id;
  }

protected:
  Ptr<Ipv4> m_ipv4; // IPv4 instance for this router
  previous_packets m_filter;

  // keep per link bandwidth information
  std::map<uint64_t, uint64_t> *bandwidth_per_link; // link is defined by the two end points
};

}
#endif
