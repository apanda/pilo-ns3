/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * PILO code PILO code PILO code
 */
#include "ipv4-pilo-dp-routing.h"
#include "ns3/log.h"
#include "ns3/names.h"
#include "ns3/packet.h"
#include "ns3/node.h"
#include "ns3/channel.h"
#include "ns3/simulator.h"
#include "ns3/ipv4-route.h"
#include "ns3/output-stream-wrapper.h"
#include "ns3/internet-module.h"

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
  switch_id = 0;
  link_states = new std::map<uint64_t, link_state>();
}

Ipv4PiloDPRouting::~Ipv4PiloDPRouting() {
  delete link_states;
}

void
Ipv4PiloDPRouting::SetSwitchId(uint32_t id) {
  this->switch_id = id;
}

uint32_t
Ipv4PiloDPRouting::GetSwitchId() {
  return this->switch_id;
}

uint64_t
Ipv4PiloDPRouting::GetLinkId(uint32_t switch_id0, uint32_t switch_id1) {

  uint64_t link_id = 0;

  uint32_t *ptr0 = (uint32_t *) &link_id;
  uint32_t *ptr1 = ((uint32_t *) &link_id)+1;
        
  *ptr0 = switch_id0 < switch_id1 ? switch_id0 : switch_id1;
  *ptr1 = switch_id0 > switch_id1 ? switch_id0 : switch_id1;
  
  return link_id;
}

uint32_t
Ipv4PiloDPRouting::GetOtherSwitchId(uint64_t link_, uint32_t switch_id) {

  uint64_t link_id = link_;

  uint32_t *ptr0 = (uint32_t *) &link_id;
  uint32_t *ptr1 = ((uint32_t *) &link_id)+1;

  uint32_t ret = (*ptr0 == switch_id) ? *ptr1 : *ptr0;
  
  return ret;
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
  if (m_routingTable.find(dest) != m_routingTable.end() &&
      m_routingTable[dest] < m_ipv4->GetNInterfaces()) {
    rtentry = Create<Ipv4Route> ();
    rtentry->SetDestination (dest);
    rtentry->SetGateway (Ipv4Address::GetZero ());
    iface = m_ipv4->GetNetDevice (m_routingTable[dest]);
    rtentry->SetOutputDevice (iface);
    rtentry->SetSource (m_ipv4->GetAddress (m_routingTable[dest], 0).GetLocal ());
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
  if (m_routingTable.find(dest) != m_routingTable.end() && 
      m_routingTable[dest] < m_ipv4->GetNInterfaces()) {
    NS_LOG_LOGIC ("Forwarding packet out interface " << m_routingTable[dest]); 
    Ptr<Ipv4Route> rtentry = 0;
    rtentry = Create<Ipv4Route> ();
    rtentry->SetDestination (header.GetDestination());
    rtentry->SetGateway (Ipv4Address::GetZero ());
    rtentry->SetOutputDevice (m_ipv4->GetNetDevice(m_routingTable[dest]));
    rtentry->SetSource (m_ipv4->GetAddress (m_routingTable[dest], 0).GetLocal ());
    ucb(rtentry, p, header);
    return true;
  }
  return false;
}

void
Ipv4PiloDPRouting::NotifyInterfaceUp(uint32_t iface) {
  // Record what is on the other side. This involves cheating, cheating is bad.
  Ptr<NetDevice> dev = m_ipv4->GetNetDevice(iface);
  if (dev->IsPointToPoint()) {
    Ptr<Channel> chan = dev->GetChannel();
    if (!chan) {
      NS_LOG_LOGIC ("No channel found for " << iface);
    } else {
      NS_ASSERT (chan->GetNDevices() == 2);
      if (chan->GetDevice(0)->GetNode() == m_ipv4->GetObject<Node>()) {
        m_ifaceToNode[iface] = chan->GetDevice(1)->GetNode()->GetId();
      } else {
        NS_ASSERT(chan->GetDevice(1)->GetNode() == m_ipv4->GetObject<Node>());
        m_ifaceToNode[iface] = chan->GetDevice(0)->GetNode()->GetId();
      }
        
      Ipv4PiloDPRoutingHelper dpRouting;
      Ptr<Ipv4PiloDPRouting> node0 = dpRouting.GetPiloDPRouting(chan->GetDevice(0)->GetNode()->GetObject<Ipv4>());
      Ptr<Ipv4PiloDPRouting> node1 = dpRouting.GetPiloDPRouting(chan->GetDevice(1)->GetNode()->GetObject<Ipv4>());
      
      if (node0 != 0 && node1 != 0) {
        
        uint32_t switch_id0 = node0->GetSwitchId();
        uint32_t switch_id1 = node1->GetSwitchId();
        uint64_t link_id = GetLinkId(switch_id0, switch_id1);
          
        if (link_states->find(link_id) == link_states->end()) {
          (*link_states)[link_id] = link_state(0, true);
        } else {
          link_state current_state = (*link_states)[link_id];
          if (!current_state.state) {
            (*link_states)[link_id].event_id = (*link_states)[link_id].event_id + 1;
            (*link_states)[link_id].state = true;
          }
        }
      }

      NS_LOG_LOGIC ("Adding mapping iface " << iface << " leads to " << m_ifaceToNode[iface]);
    }
      
  } else {
    NS_LOG_LOGIC ("Ignoring non-point-to-point device " << iface);
  }
}

void
Ipv4PiloDPRouting::NotifyInterfaceDown(uint32_t iface) {
  NS_LOG_LOGIC("Link is down");
  // change the state here
  Ptr<NetDevice> dev = m_ipv4->GetNetDevice(iface);
  Ptr<Channel> chan = dev->GetChannel();

  Ipv4PiloDPRoutingHelper dpRouting;
  Ptr<Ipv4PiloDPRouting> node0 = dpRouting.GetPiloDPRouting(chan->GetDevice(0)->GetNode()->GetObject<Ipv4>());
  Ptr<Ipv4PiloDPRouting> node1 = dpRouting.GetPiloDPRouting(chan->GetDevice(1)->GetNode()->GetObject<Ipv4>());
  
  if (node0 != 0 && node1 != 0) {
    uint32_t switch_id0 = node0->GetSwitchId();
    uint32_t switch_id1 = node1->GetSwitchId();
    uint64_t link_id = GetLinkId(switch_id0, switch_id1);
    
    if (link_states->find(link_id) != link_states->end()) {
      link_state current_state = (*link_states)[link_id];
      if (current_state.state) {
        (*link_states)[link_id].event_id = (*link_states)[link_id].event_id + 1;
        (*link_states)[link_id].state = false;

        NS_LOG_LOGIC("Link down between switch " << switch_id0 << " and switch " << switch_id1);
      }
    }
  }
  
  // Need to regenerate the m_ifaceToNode table, since ifaces change
  m_ifaceToNode.clear();
  for (uint32_t i = 0; i < m_ipv4->GetNInterfaces(); i++) {
    // Add a new entry
    NotifyInterfaceUp(i);
  }
}

void
Ipv4PiloDPRouting::NotifyAddAddress(uint32_t iface, Ipv4InterfaceAddress address) {
  // Do we need this? What is the use of keeping track of addresses for a node.
  m_addressIface[address.GetLocal()] = iface;
}

void
Ipv4PiloDPRouting::NotifyRemoveAddress(uint32_t iface, Ipv4InterfaceAddress address) {
  m_addressIface.erase(address.GetLocal());
}

void
Ipv4PiloDPRouting::SetIpv4(Ptr<Ipv4> ipv4) {
  m_ipv4 = ipv4;
  TypeId tid = TypeId::LookupByName ("ns3::PiloSocketFactory");
  if (m_socket != 0) {
    m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
  }
  m_socket = DynamicCast<PiloSocket>(Socket::CreateSocket(m_ipv4->GetObject<Node> (), tid));
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (),
                                             PORT);
  m_socket->Bind (local);
  m_socket->SetRecvCallback(MakeCallback(&Ipv4PiloDPRouting::HandleRead, this));
}

void 
Ipv4PiloDPRouting::PrintRoutingTable (Ptr<OutputStreamWrapper> stream) const {
  std::ostream* os = stream->GetStream ();
  *os << "Address    Interface" << std::endl;
  for (RoutingTable::const_iterator it = m_routingTable.cbegin();
                                it != m_routingTable.cend(); it++) {
    *os << it->first << "  " << it->second << std::endl;
  }
}

void 
Ipv4PiloDPRouting::HandlePiloControlPacket (const PiloHeader& hdr, Ptr<Packet> pkt) {
  //NS_LOG_FUNCTION (this << hdr << pkt);
  switch(hdr.GetType()) {
    case NOP:
      NS_LOG_LOGIC ("Received NOP");
      break;
    case Echo:
      char buf[1024];
      pkt->CopyData((uint8_t*)buf, 1024);
      NS_LOG_LOGIC ("Received echo " << buf);
      // Send echo back
      if (m_socket->SendPiloMessage(hdr.GetSourceNode(), EchoAck, pkt) < 0) {
        NS_LOG_LOGIC("Error responding to echo");
      } else {
        NS_LOG_LOGIC("Sent an echo ack");
      }
      break;
    case EchoAck:
      NS_LOG_LOGIC("Received EchoAck ");
      // Ignore EchoAcks
      break;
    case AddRoute:
      break;
    case LinkState:
      {
        // send to all controllers the current link states
        std::map<uint64_t, link_state>::iterator it = link_states->begin();
        std::map<uint64_t, link_state>::iterator it_end = link_states->end();
        
        for (; it != it_end; it++) {
          InterfaceStateMessage m;
          
          m.switch_id = this->switch_id;
          m.other_switch_id = GetOtherSwitchId(it->first, this->switch_id);
          m.link_id = it->first;
          m.event_id = it->second.event_id;
          m.state = it->second.state;
          
          Ptr<Packet> p = Create<Packet> ((uint8_t *) &m, sizeof(m));
          if (m_socket->SendPiloMessage(PiloHeader::ALL_NODES, LinkStateReply, p) < 0) {
            NS_LOG_LOGIC("Error sending link state reply");
          }
        }
      }
      break;
    // case LinkStateReply:
    //   break;
    // case GossipRequest:
    //   break;
    // case GossipReply:
    //   break;
    default:
      break;
  };

}
// Called when a PILO control packet is received over the control connection.
void 
Ipv4PiloDPRouting::HandleRead (Ptr<Socket> socket) {
  //NS_LOG_FUNCTION (this << socket);
  Ptr<Packet> packet;
  Address from;
  PiloHeader piloHeader;
  while ((packet = socket->RecvFrom (from))) {
    //NS_LOG_LOGIC("HandleRead packet " << packet->GetSize());
    if (packet->GetSize() > 0) {
      packet->RemoveHeader(piloHeader);
      //NS_LOG_LOGIC("Processing actual PILO packet " << piloHeader);
      if (piloHeader.GetTargetNode() == PiloHeader::ALL_NODES ||
          piloHeader.GetTargetNode() == m_ipv4->GetObject<Node>()->GetId()) {
        HandlePiloControlPacket(piloHeader, packet);
      } else {
        //NS_LOG_LOGIC ("Ignoring PILO packet not intended for me.");
      }
    } else {
      //NS_LOG_LOGIC("Zero-size control packet");
    }
  }
}

} // namespace ns3
