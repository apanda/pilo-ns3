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
#include "ns3/pilo-header.h"
#include "ns3/ipv4-pilo-dp-routing-helper.h"

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
  NS_LOG_FUNCTION (this);
  bandwidth_per_link = new std::map<uint64_t, uint64_t>();
}

  Ipv4PiloCtlRouting::~Ipv4PiloCtlRouting() {
    delete bandwidth_per_link;
  }

Ptr<Ipv4Route> 
Ipv4PiloCtlRouting::RouteOutput (Ptr<Packet> p, 
                                 const Ipv4Header &header, 
                                 Ptr<NetDevice> oif, 
                                 Socket::SocketErrno &sockerr) {
  NS_LOG_FUNCTION (this);
  Ptr<Ipv4Route> rtentry = 0;
  // We only deal with PILO control packets here.
  if (header.IsPiloControl()) {
    NS_LOG_LOGIC ("Building a routing entry");
    uint32_t ifIndex = 0;
    if (!oif) { // Just pick any interface. We don't care much about which one.
      if (m_ipv4->GetNInterfaces() > 1) {
        ifIndex = 1; // Interface 0 is always loopback.
      } else {
        ifIndex = 0;
      }
      oif = m_ipv4->GetNetDevice(ifIndex);
    } else { // OK we can go along with the passed in OIF.
      ifIndex = oif->GetIfIndex();
    }
    rtentry = Create<Ipv4Route> ();
    rtentry->SetDestination (header.GetDestination());
    rtentry->SetGateway (Ipv4Address::GetZero ());
    rtentry->SetOutputDevice (oif);
    rtentry->SetSource (m_ipv4->GetAddress (ifIndex, 0).GetLocal ());
    rtentry->SetBroadcast(true);
    sockerr = Socket::ERROR_NOTERROR;
    NS_LOG_LOGIC ("Done, sending to " << rtentry->GetDestination() << 
                  " through "  << rtentry->GetGateway()<<
                  " device " << oif);
  } else {
    sockerr = Socket::ERROR_NOROUTETOHOST;
    NS_LOG_LOGIC("RouteOutput not sending since not a PILO control packet");
  }
  return rtentry;
}

bool 
Ipv4PiloCtlRouting::RouteInput(Ptr<const Packet> p, 
                  const Ipv4Header &header, Ptr<const NetDevice> idev, 
                  UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                  LocalDeliverCallback lcb, ErrorCallback ecb) {
  NS_LOG_FUNCTION (this);
  if (!header.IsPiloControl()) {
    NS_LOG_LOGIC ("Not forwarding packet, not a PILO control packet");
    return false; // If not a PILO control packet then we don't care about forwarding.
  }

  PiloHeader piloHeader; // We always expect a PILO header
  p->PeekHeader (piloHeader);

  NS_LOG_LOGIC("Handling packet with header " << piloHeader);
  NS_LOG_LOGIC("Handling packet from " << piloHeader.GetSourceNode() << ", message type " << piloHeader.GetType());

  if (m_filter.find(piloHeader.GetSourceNode()) != m_filter.end() &&
      m_filter[piloHeader.GetSourceNode()].find(header.GetIdentification())  != 
         m_filter[piloHeader.GetSourceNode()].end()) {
    NS_LOG_LOGIC ("Not forwarding packet, is duplicate");
    return true; // We have already forwarded this packet. Not only should we not forward it, we need to make sure that
                 // list routing or other things don't pass it to the next node.
  }


  Ptr<Channel> chan = idev->GetChannel();
  Ptr<Node> node0 = chan->GetDevice(0)->GetNode();
  Ptr<Node> node1 = chan->GetDevice(1)->GetNode();
  Ipv4PiloDPRoutingHelper dpRouting;

  uint32_t node0_id = 0;
  uint32_t node1_id = 0;

  NS_ASSERT(node0 != 0);
  NS_ASSERT(node1 != 0);

  Ptr<Ipv4PiloDPRouting> dp_node0 = dpRouting.GetPiloDPRouting(node0->GetObject<Ipv4>());
  Ptr<Ipv4PiloDPRouting> dp_node1 = dpRouting.GetPiloDPRouting(node1->GetObject<Ipv4>());

  if (dp_node0 != 0) {
    node0_id = dp_node0->GetSwitchId();
  } else {
    node0_id = node0->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal().Get();
  }

  if (dp_node1 != 0) {
    node1_id = dp_node1->GetSwitchId();
  } else {
    node1_id = node1->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal().Get();
  }

  uint64_t link_id = GetLinkId(node0_id, node1_id);
  if (bandwidth_per_link->find(link_id) == bandwidth_per_link->end()) {
    (*bandwidth_per_link)[link_id] = 0;
  }
  (*bandwidth_per_link)[link_id] += p->GetSize();
      
  m_filter[piloHeader.GetSourceNode()].insert(header.GetIdentification());
  // When flooding don't send out received port
  uint32_t iifIdx = m_ipv4->GetInterfaceForDevice(idev);


  // Locally deliver if necessary
  // Local delivery is unnecessary, PiloSocket is raw and in promiscuous mode, so it
  // delivers everything
  //if (piloHeader.GetTargetNode() == PiloHeader::ALL_NODES) {
    //NS_LOG_LOGIC ("Locally delivering packet");
    //lcb(p, header, iifIdx);
  //}

  //if (piloHeader.GetTargetNode() == m_ipv4->GetObject<Node>()->GetId()) {
    //NS_LOG_LOGIC ("Locally delivering packet");
    //lcb(p, header, iifIdx);
    //NS_LOG_LOGIC ("Not forwarding anymore, since delivered");
    //return true;
  //}
  
  // Flood out all interfaces (except for loopback).
  for (uint32_t i = 1; i < m_ipv4->GetNInterfaces(); i++) {
    if (i != iifIdx) {
      NS_LOG_LOGIC ("Forwarding packet out interface " << i); 
      Ptr<Ipv4Route> rtentry = 0;
      rtentry = Create<Ipv4Route> ();
      rtentry->SetDestination (header.GetDestination());
      rtentry->SetGateway (Ipv4Address::GetZero ());
      rtentry->SetOutputDevice (m_ipv4->GetNetDevice(i));
      rtentry->SetSource (m_ipv4->GetAddress (i, 0).GetLocal ());
      ucb(rtentry, p->Copy(), header);
    } else {
      NS_LOG_LOGIC ("Not forwarding packet out interface " << i << " since received"); 
    }
  }
  return true;
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
