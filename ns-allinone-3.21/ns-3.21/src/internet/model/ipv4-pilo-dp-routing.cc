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
  hosts = new std::set<uint32_t>();
}

Ipv4PiloDPRouting::~Ipv4PiloDPRouting() {
  delete link_states;
  delete hosts;
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
  NS_LOG_LOGIC ("Attempting to get routing table entry for destination address " << dest.Get());
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
  NS_LOG_LOGIC ("Attempting to get routing table entry for destination address " << dest.Get());
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

  SendLinkStates();
}

void
Ipv4PiloDPRouting::NotifyInterfaceDown(uint32_t iface) {
  NS_LOG_LOGIC("Link is down");
  // change the state here
  Ptr<NetDevice> dev = m_ipv4->GetNetDevice(iface);
  Ptr<Channel> chan = dev->GetChannel();

  Ipv4StaticRoutingHelper sRouting;
  Ipv4PiloDPRoutingHelper dpRouting;
  Ptr<Ipv4StaticRouting> s_node0 = sRouting.GetStaticRouting(chan->GetDevice(0)->GetNode()->GetObject<Ipv4>());
  if (s_node0 != 0) {
    // node 0 is a host; node 1 must be a switch
    uint32_t host_ip = s_node0->GetIP();
    // store host_ip
    hosts->erase(host_ip);
  } else {
    
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
          
          NS_LOG_LOGIC("Link down between switch " << switch_id0 << " and switch " << switch_id1 << 
                       " for link id " << link_id);
        }
      }
      
    }
  }

  SendLinkStates();
  
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
      {
        Ipv4PiloDPRoutingHelper dpRouting;
        
        uint8_t add_route_buf[8];
        pkt->CopyData(add_route_buf, 8);

        uint32_t *host_addr_ptr = (uint32_t *) (add_route_buf);
        uint32_t host_addr = *host_addr_ptr;
        uint32_t *route_node_id_ptr = (uint32_t *) (add_route_buf + 4);
        uint32_t route_node_id = *route_node_id_ptr;

        NS_LOG_LOGIC("AddRoute called at switch " << this->switch_id << " with host_addr " << host_addr << ", route_node_id " << 
                     route_node_id);

        for (uint32_t i = 0; i < m_ipv4->GetNInterfaces(); i++) {
        
          Ptr<NetDevice> dev = m_ipv4->GetNetDevice(i);
          Ptr<Channel> chan = dev->GetChannel();
          if (chan != 0) {
            Ptr<Node> node0 = chan->GetDevice(0)->GetNode();
            Ptr<Node> node1 = chan->GetDevice(1)->GetNode();
            
            if (node0 == m_ipv4->GetObject<Node>()) {
              // node1 could be a host
              // check address
              Ptr<Ipv4PiloDPRouting> dp_node1 = dpRouting.GetPiloDPRouting(node1->GetObject<Ipv4>());
              if (dp_node1 != 0) {
                if (dp_node1->switch_id == route_node_id) {
                  NS_LOG_LOGIC("Destination " << host_addr << " routed through interface " << i << " at switch " << this->switch_id
                               << ", other switch is " << dp_node1->switch_id);
                  m_routingTable[Ipv4Address(host_addr)] = i;
                }
              } else {
                // node1 is a host, check to see if it's the target host node
                uint32_t addr = node1->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal().Get();
                //NS_LOG_LOGIC("Node 1 is host with address " << addr);
                if (addr == host_addr) {
                  m_routingTable[Ipv4Address(host_addr)] = i;
                  NS_LOG_LOGIC("Destination " << host_addr << " routed directly to host through interface " << i << " at switch " << this->switch_id);
                }
              }
            } else {
              Ptr<Ipv4PiloDPRouting> dp_node0 = dpRouting.GetPiloDPRouting(node0->GetObject<Ipv4>());
              if (dp_node0 != 0) {
                if (dp_node0->switch_id == route_node_id) {
                  NS_LOG_LOGIC("Destination " << host_addr << " routed through interface " << i << " at switch " << this->switch_id
                               << ", other switch is " << dp_node0->switch_id);
                  m_routingTable[Ipv4Address(host_addr)] = i;
                }
              } else {
                // node0 is a host, check to see if it's the target host node
                uint32_t addr = node0->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal().Get();
                //NS_LOG_LOGIC("Node 0 is host with address " << addr);
                if (addr == host_addr) {
                  m_routingTable[Ipv4Address(host_addr)] = i;
                  NS_LOG_LOGIC("Destination " << host_addr << " routed directly to host through interface " << i << " at switch " << this->switch_id);
                }
              }
            }
          }
        } 
      }
      break;
    case LinkState:
      {
        SendLinkStates();
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

  void Ipv4PiloDPRouting::SendLinkStates() {
    hosts->clear();
    // find the hosts connected to this switch
    Ipv4StaticRoutingHelper sRouting;

    //NS_LOG_LOGIC("Switch " << switch_id << " has " << m_ipv4->GetNInterfaces() << " interfaces ");
    for (uint32_t i = 0; i < m_ipv4->GetNInterfaces(); i++) {
      Ptr<NetDevice> dev = m_ipv4->GetNetDevice(i);
      Ptr<Channel> chan = dev->GetChannel();
      if (chan == 0) {
        continue;
      }
      Ptr<Node> node0 = chan->GetDevice(0)->GetNode();
      if (node0 == 0) {
        // not set up?
        continue;
      }

      Ptr<Node> this_node = m_ipv4->GetObject<Node>();
      NS_ASSERT(this_node != 0);

      if (node0 == this_node) {
        Ptr<Node> node1 = chan->GetDevice(1)->GetNode();
        if (node1 != 0) {
          // see if this node is host
          Ptr<Ipv4StaticRouting> sr = sRouting.GetStaticRouting(node1->GetObject<Ipv4>());
              
          if (sr) {
            // is a host
            if (node1->GetObject<Ipv4>() != 0) {
              Ptr<Ipv4> ip = node1->GetObject<Ipv4>();
              if (ip->GetNInterfaces() > 1 && ip->GetNAddresses(1) > 0) {
                uint32_t addr = node1->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal().Get();
                hosts->insert(addr);
              }
            }
          }
        }
      } else {
        Ptr<Node> node1 = chan->GetDevice(1)->GetNode();
        NS_ASSERT(node1 == this_node);
        Ptr<Ipv4StaticRouting> sr = sRouting.GetStaticRouting(node0->GetObject<Ipv4>());
            
        if (sr) {
          // is host
          if (node0->GetObject<Ipv4>() != 0) {
            Ptr<Ipv4> ip = node1->GetObject<Ipv4>();
            if (ip->GetNInterfaces() > 1 && ip->GetNAddresses(1) > 0) {
              uint32_t addr = node0->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal().Get();
              hosts->insert(addr);
            }
          }
        }
      }
    }
    

    // send to all controllers the current link states
    std::map<uint64_t, link_state>::iterator it = link_states->begin();
    std::map<uint64_t, link_state>::iterator it_end = link_states->end();
        
    const size_t total_size = (const size_t) (link_states->size() * sizeof(InterfaceStateMessage) + sizeof(uint32_t) + hosts->size() * sizeof(InterfaceStateMessage));

    uint8_t buf[total_size];
    uint8_t *buf_ptr = buf;

    // first store the switch_id
    uint32_t *switch_id_ptr = (uint32_t *) buf_ptr;
    *switch_id_ptr  = this->switch_id;
    buf_ptr += sizeof(uint32_t);

    // // store IP of this switch
    // Ipv4Address *addr = (Ipv4Address *) buf_ptr;
    // *addr = m_ipv4->GetAddress(1, 0).GetLocal();
    // buf_ptr += sizeof(Ipv4Address);
        
    int counter = 0;
        
    for (; it != it_end; it++) {
      InterfaceStateMessage *m = (InterfaceStateMessage *) (buf_ptr + sizeof(InterfaceStateMessage) * counter);
          
      m->switch_id = this->switch_id;
      m->other_switch_id = GetOtherSwitchId(it->first, this->switch_id);
      m->link_id = it->first;
      m->event_id = it->second.event_id;
      m->state = it->second.state;
      m->is_host = false;
      counter++;
    }

    // also send over hosts information
    std::set<uint32_t>::iterator h_it = hosts->begin(); 
    std::set<uint32_t>::iterator h_it_end = hosts->end();

    for (; h_it != h_it_end; h_it++) {
      InterfaceStateMessage *m = (InterfaceStateMessage *) (buf_ptr + sizeof(InterfaceStateMessage) * counter);
      m->switch_id = this->switch_id;
      m->other_switch_id = *h_it;
      m->link_id = 0;
      m->event_id = 0;
      m->state = true;
      m->is_host = true;

      counter++;
    }

    Ptr<Packet> p = Create<Packet> (buf, total_size);
    if (m_socket->SendPiloMessage(PiloHeader::ALL_NODES, LinkStateReply, p) < 0) {
      NS_LOG_LOGIC("Error sending link state reply");
    }
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
