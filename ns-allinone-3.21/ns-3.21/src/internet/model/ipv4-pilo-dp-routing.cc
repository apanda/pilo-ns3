/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * PILO code PILO code PILO code
 */
#include "ipv4-pilo-dp-routing.h"
#include "ns3/log.h"
#include "ns3/names.h"
#include "ns3/packet.h"
#include "ns3/node.h"
#include "ns3/simulator.h"
#include "ns3/ipv4-route.h"
#include "ns3/output-stream-wrapper.h"

NS_LOG_COMPONENT_DEFINE ("Ipv4PiloDPRouting");
namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(Ipv4PiloDPRouting);

TypeId 
Ipv4PiloDPRouting::GetTypeId (void) {
  static TypeId tid = TypeId ("ns3::Ipv4PiloDPRouting")
    .SetParent<Ipv4RoutingProtocol> ()
    .AddConstructor<Ipv4PiloDPRouting> ()
  ;
  return tid;
}

Ipv4PiloDPRouting::Ipv4PiloDPRouting ():
    m_ipv4(NULL) {
  NS_LOG_FUNCTION (this);
}

Ptr<Ipv4Route> 
Ipv4PiloDPRouting::RouteOutput (Ptr<Packet> p, 
                                 const Ipv4Header &header, 
                                 Ptr<NetDevice> oif, 
                                 Socket::SocketErrno &sockerr) {
  NS_LOG_FUNCTION (this);
  Ptr<Ipv4Route> rtentry = 0;
  Ipv4Address dest = header.GetDestination ();
  Ptr<NetDevice> iface = 0;
  if (m_table.find(dest) != m_table.end() &&
      m_table[dest] < m_ipv4->GetNInterfaces()) {
    rtentry = Create<Ipv4Route> ();
    rtentry->SetDestination (dest);
    rtentry->SetGateway (Ipv4Address::GetZero ());
    iface = m_ipv4->GetNetDevice (m_table[dest]);
    rtentry->SetOutputDevice (iface);
    rtentry->SetSource (m_ipv4->GetAddress (m_table[dest], 0).GetLocal ());
  }
  return rtentry;
}

bool 
Ipv4PiloDPRouting::RouteInput(Ptr<const Packet> p, 
                  const Ipv4Header &header, Ptr<const NetDevice> idev, 
                  UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                  LocalDeliverCallback lcb, ErrorCallback ecb) {
  NS_LOG_FUNCTION (this);
  // Check if this packet is meant for local delivery.
  uint32_t iif = m_ipv4->GetInterfaceForDevice (idev); 
  for (uint32_t j = 0; j < m_ipv4->GetNInterfaces (); j++) {
    for (uint32_t i = 0; i < m_ipv4->GetNAddresses (j); i++) {
      Ipv4InterfaceAddress iaddr = m_ipv4->GetAddress (j, i);
      Ipv4Address addr = iaddr.GetLocal ();
      if (addr.IsEqual (header.GetDestination ())) {
          if (j == iif) {
            NS_LOG_LOGIC ("For me (destination " << addr << " match)");
          }
          else {
            NS_LOG_LOGIC ("For me (destination " << addr << 
                          " match) on another interface " << header.GetDestination ());
          }
          lcb (p, header, iif);
          return true;
      }
      if (header.GetDestination().IsEqual(iaddr.GetBroadcast ())) {
          NS_LOG_LOGIC ("For me (interface broadcast address)");
          lcb(p, header, iif);
          return true;
      }
      NS_LOG_LOGIC ("Address "<< addr << " not a match");
    }
  }

  // Check if we have a routing entry
  Ipv4Address dest = header.GetDestination();
  if (m_table.find(dest) != m_table.end() && 
      m_table[dest] < m_ipv4->GetNInterfaces()) {
    NS_LOG_LOGIC ("Forwarding packet out interface " << m_table[dest]); 
    Ptr<Ipv4Route> rtentry = 0;
    rtentry = Create<Ipv4Route> ();
    rtentry->SetDestination (header.GetDestination());
    rtentry->SetGateway (Ipv4Address::GetZero ());
    rtentry->SetOutputDevice (m_ipv4->GetNetDevice(m_table[dest]));
    rtentry->SetSource (m_ipv4->GetAddress (m_table[dest], 0).GetLocal ());
    ucb(rtentry, p, header);
    return true;
  }
  return false;
}

void
Ipv4PiloDPRouting::NotifyInterfaceUp(uint32_t iface) {
  // Routing here just involves flooding, we don't need this.
}

void
Ipv4PiloDPRouting::NotifyInterfaceDown(uint32_t iface) {
  // Routing here just involves flooding, we don't need this.
}

void
Ipv4PiloDPRouting::NotifyAddAddress(uint32_t iface, Ipv4InterfaceAddress address) {
  // Don't need
}

void
Ipv4PiloDPRouting::NotifyRemoveAddress(uint32_t iface, Ipv4InterfaceAddress address) {
  // Don't need
}

void
Ipv4PiloDPRouting::SetIpv4(Ptr<Ipv4> ipv4) {
  m_ipv4 = ipv4;
  TypeId tid = TypeId::LookupByName ("ns3::PiloSocketFactory");
  m_socket = Socket::CreateSocket (m_ipv4->GetObject<Node> (), tid);
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (),
                                             6500);
  m_socket->Bind (local);
}

void 
Ipv4PiloDPRouting::PrintRoutingTable (Ptr<OutputStreamWrapper> stream) const {
  *(stream->GetStream()) << "PILO CTL packets flooded " << std::endl;
}

} // namespace ns3
