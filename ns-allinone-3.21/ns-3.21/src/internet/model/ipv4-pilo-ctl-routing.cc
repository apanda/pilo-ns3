/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * PILO code PILO code PILO code
 */
#include "ipv4-pilo-ctl-routing.h"
#include "ns3/log.h"
#include "ns3/names.h"
#include "ns3/packet.h"
#include "ns3/node.h"
#include "ns3/simulator.h"
#include "ns3/ipv4-route.h"
#include "ns3/output-stream-wrapper.h"

NS_LOG_COMPONENT_DEFINE ("Ipv4PiloCtlRouting");
namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(Ipv4PiloCtlRouting);

TypeId 
Ipv4PiloCtlRouting::GetTypeId (void) {
  static TypeId tid = TypeId ("ns3::Ipv4PiloCtlRouting")
    .SetParent<Ipv4RoutingProtocol> ()
    .AddConstructor<Ipv4PiloCtlRouting> ()
  ;
  return tid;
}

Ipv4PiloCtlRouting::Ipv4PiloCtlRouting ():
    m_ipv4(NULL) {
}

Ptr<Ipv4Route> 
Ipv4PiloCtlRouting::RouteOutput (Ptr<Packet> p, 
                                 const Ipv4Header &header, 
                                 Ptr<NetDevice> oif, 
                                 Socket::SocketErrno &sockerr) {
  return NULL;
}

bool 
Ipv4PiloCtlRouting::RouteInput(Ptr<const Packet> p, 
                  const Ipv4Header &header, Ptr<const NetDevice> idev, 
                  UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                  LocalDeliverCallback lcb, ErrorCallback ecb) {
  return false;
}

void
Ipv4PiloCtlRouting::NotifyInterfaceUp(uint32_t iface) {
  // Routing here just involves flooding, we don't need this.
}

void
Ipv4PiloCtlRouting::NotifyInterfaceDown(uint32_t iface) {
  // Routing here just involves flooding, we don't need this.
}

void
Ipv4PiloCtlRouting::NotifyAddAddress(uint32_t iface, Ipv4InterfaceAddress address) {
  // Don't need
}

void
Ipv4PiloCtlRouting::NotifyRemoveAddress(uint32_t iface, Ipv4InterfaceAddress address) {
  // Don't need
}

void
Ipv4PiloCtlRouting::SetIpv4(Ptr<Ipv4> ipv4) {
  m_ipv4 = ipv4;
}

void 
Ipv4PiloCtlRouting::PrintRoutingTable (Ptr<OutputStreamWrapper> stream) const {
  *(stream->GetStream()) << "PILO CTL packets flooded " << std::endl;
}

} // namespace ns3
