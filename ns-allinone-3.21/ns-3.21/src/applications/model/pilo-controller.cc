/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007,2008,2009 INRIA, UDCAST
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Amine Ismail <amine.ismail@sophia.inria.fr>
 *                      <amine.ismail@udcast.com>
 */
#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "pilo-controller.h"
#include "seq-ts-header.h"
#include "ns3/pilo-header.h"
#include <cstdlib>
#include <cstdio>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PiloController");
NS_OBJECT_ENSURE_REGISTERED (PiloController);

TypeId
PiloController::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PiloController")
    .SetParent<Application> ()
    .AddConstructor<PiloController> ()
    .AddAttribute ("MaxPackets",
                   "The maximum number of packets the application will send",
                   UintegerValue (100),
                   MakeUintegerAccessor (&PiloController::m_count),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Interval",
                   "The time to wait between packets", TimeValue (Seconds (1.0)),
                   MakeTimeAccessor (&PiloController::m_interval),
                   MakeTimeChecker ())
    .AddAttribute ("RemoteAddress",
                   "The destination Address of the outbound packets",
                   AddressValue (),
                   MakeAddressAccessor (&PiloController::m_peerAddress),
                   MakeAddressChecker ())
    .AddAttribute ("RemotePort", "The destination port of the outbound packets",
                   UintegerValue (100),
                   MakeUintegerAccessor (&PiloController::m_peerPort),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("PacketSize",
                   "Size of packets generated. The minimum packet size is 12 bytes which is the size of the header carrying the sequence number and the time stamp.",
                   UintegerValue (1024),
                   MakeUintegerAccessor (&PiloController::m_size),
                   MakeUintegerChecker<uint32_t> (12,1500))
    .AddAttribute ("NodeSend",
                   "What node to send PILO packets",
                   UintegerValue (PiloHeader::ALL_NODES),
                   MakeUintegerAccessor (&PiloController::m_targetNode),
                   MakeUintegerChecker<uint32_t> ())
  ;
  return tid;
}

PiloController::PiloController ()
{
  NS_LOG_FUNCTION (this);
  m_sent = 0;
  m_socket = 0;
  m_sendEvent = EventId ();
  
  messages = new std::map<uint32_t, uint32_t>();
  mapping = new std::map<uint32_t, uint32_t>();
  hosts = new std::map<uint32_t, std::vector<uint32_t> *>();

  log = new ControllerState();
  gossip_send_counter = 0;
  link_state_send_counter = 0;
  max_counter = 20;
}

PiloController::~PiloController ()
{
  NS_LOG_FUNCTION (this);
  delete messages;
  delete log;
  delete mapping;
  
  std::map<uint32_t, std::vector<uint32_t> *>::iterator it = hosts->begin();
  std::map<uint32_t, std::vector<uint32_t> *>::iterator it_end = hosts->end();

  for (; it != it_end; it++) {
    delete it->second;
  }
  delete hosts;
}

void
PiloController::SetRemote (Ipv4Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_peerAddress = Address(ip);
  m_peerPort = port;
}

void
PiloController::SetRemote (Ipv6Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_peerAddress = Address(ip);
  m_peerPort = port;
}

void
PiloController::SetRemote (Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_peerAddress = ip;
  m_peerPort = port;
}

void
PiloController::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Application::DoDispose ();
}

void
PiloController::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::PiloSocketFactory");
      m_socket = DynamicCast<PiloSocket>(Socket::CreateSocket (GetNode (), tid));
      if (Ipv4Address::IsMatchingType(m_peerAddress) == true)
        {
          m_socket->Bind ();
          //m_socket->Connect (InetSocketAddress (Ipv4Address::ConvertFrom(m_peerAddress), m_peerPort));
        }
      else if (Ipv6Address::IsMatchingType(m_peerAddress) == true)
        {
          m_socket->Bind6 ();
          m_socket->Connect (Inet6SocketAddress (Ipv6Address::ConvertFrom(m_peerAddress), m_peerPort));
        }
    }

  //m_sendEvent = Simulator::Schedule(Seconds(0.0), &PiloController::CtlGossip, this);
  m_socket->SetRecvCallback (MakeCallback(&PiloController::HandleRead, this));

  // periodically query for link states from switches
  m_sendEvent = Simulator::Schedule (Seconds (0.0), &PiloController::GetLinkState, this);
  // start gossiping
  Simulator::Schedule (Seconds (0.0), &PiloController::CtlGossip, this);
  // start gc
  Simulator::Schedule (Seconds (0.1), &PiloController::GarbageCollect, this);
  // start route assignment function
  Simulator::Schedule(Seconds(0.1), &PiloController::AssignRoutes, this);

  Simulator::Schedule(Seconds(final_time+5), &PiloController::CurrentLog, this);
}

void
PiloController::StopApplication (void)
{
  NS_LOG_FUNCTION (this);
  Simulator::Cancel (m_sendEvent);
}

void
PiloController::Send (void)
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT (m_sendEvent.IsExpired ());
  //PiloHeader hdr(GetNode()->GetId(), m_targetNode, Echo); 
  Ptr<Packet> p = Create<Packet> ((uint8_t*)"Hello", 6);
  //p->AddHeader(hdr);

  std::stringstream peerAddressStringStream;
  peerAddressStringStream << Ipv4Address::ConvertFrom (m_peerAddress);

  if ((m_socket->SendPiloMessage(m_targetNode, Echo, p)) >= 0)
    {
      ++m_sent;
      NS_LOG_INFO ("TraceDelay TX " << m_size << " bytes to "
                                    << peerAddressStringStream.str () << " Uid: "
                                    << p->GetUid () << " Time: "
                                    << (Simulator::Now ()).GetSeconds () << " Seq: ");
                                    //<< seqTs.GetSeq() << " "
                                    //<< hdr);

    }
  else
    {
      NS_LOG_INFO ("Error while sending " << m_size << " bytes to "
                                          << peerAddressStringStream.str ());
    }

  if (m_sent < m_count)
    {
      m_sendEvent = Simulator::Schedule (m_interval, &PiloController::Send, this);
    }
}


void
PiloController::GetLinkState (void)
{
  NS_LOG_FUNCTION (this);
  //NS_ASSERT (m_sendEvent.IsExpired ());
  //PiloHeader hdr(GetNode()->GetId(), m_targetNode, Echo); 
  Ptr<Packet> p = Create<Packet> ((uint8_t*)"Hello", 6);
  //p->AddHeader(hdr);

  std::stringstream peerAddressStringStream;
  peerAddressStringStream << Ipv4Address::ConvertFrom (m_peerAddress);

  if ((m_socket->SendPiloMessage(PiloHeader::ALL_NODES, LinkState, p)) >= 0)
    {
      ++m_sent;
      NS_LOG_INFO ("TraceDelay TX " << m_size << " bytes to "
                                    << peerAddressStringStream.str () << " Uid: "
                                    << p->GetUid () << " Time: "
                                    << (Simulator::Now ()).GetSeconds () << " Seq: ");

    }
  else
    {
      NS_LOG_INFO ("Error while sending " << m_size << " bytes to "
                                          << peerAddressStringStream.str ());
    }

  // reschedule itself
  if (Simulator::Now().Compare(Seconds(final_time)) < 0) {
    Simulator::Schedule (Seconds(0.5), &PiloController::GetLinkState, this);
    link_state_send_counter++;
  }
}

  void
  PiloController::HandleRead (Ptr<Socket> socket)
  {
    NS_LOG_FUNCTION (this << socket);
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom (from)))
      {
        if (packet->GetSize () > 0)
          {
            PiloHeader piloHdr;
            packet->RemoveHeader (piloHdr);
            if (piloHdr.GetType() == EchoAck) {
              NS_LOG_LOGIC("Received EchoAck from " << piloHdr.GetSourceNode());
              NS_ASSERT(piloHdr.GetTargetNode() == GetNode()->GetId());
            } else if (piloHdr.GetType() == GossipRequest) {
              std::set<LinkEvent *, SortLinkEvent> *result = new std::set<LinkEvent *, SortLinkEvent>();

              uint8_t *buf = (uint8_t *) malloc(packet->GetSize());
              packet->CopyData(buf, packet->GetSize());

              uint8_t *buf_ptr = buf;
              
              size_t total_bytes = 0;
              
              // iterate through event gaps
              while (total_bytes < packet->GetSize()) {

                uint32_t *switch_id_ptr = (uint32_t *) buf_ptr;
                uint32_t switch_id = *switch_id_ptr;
                buf_ptr += sizeof(uint32_t);

                uint64_t *link_id_ptr = (uint64_t *) buf_ptr;
                uint64_t link_id = *link_id_ptr;
                buf_ptr += sizeof(uint64_t);

                uint32_t *num_events_ptr = (uint32_t *) buf_ptr;
                uint32_t num_events = *num_events_ptr;
                buf_ptr += sizeof(uint32_t);

                // NS_LOG_LOGIC("[GossipReply] Server " << GetNode()->GetId() << " switch " << switch_id << ", link " << link_id
                //              << " has " << num_events << " events");

                total_bytes += sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint32_t);

                for (uint32_t i = 0; i < num_events; i++) {
                  uint64_t *event_id0_ptr = (uint64_t *) buf_ptr;
                  uint64_t event_id0 = *event_id0_ptr;
                  buf_ptr += sizeof(uint64_t);
                  total_bytes += sizeof(uint64_t);
                
                  uint64_t *event_id1_ptr = (uint64_t *) buf_ptr;
                  uint64_t event_id1 = *event_id1_ptr;
                  buf_ptr += sizeof(uint64_t);
                  total_bytes += sizeof(uint64_t);

                  //NS_LOG_LOGIC("[GossipReply] Server " << GetNode()->GetId() << " switch " << switch_id << ", link " << link_id << ", found gap [" << event_id0 << ", " << event_id1 << "]");
                
                  if (event_id0 == event_id1) {
                    log->get_greater_events(switch_id, link_id, event_id0, result);
                  } else {
                    log->get_events_within_gap(switch_id, link_id, event_id0, event_id1, result);
                  }
                }
              }

              // TODO: send over the result
              size_t msg_size = sizeof(LinkEvent) * result->size();
              uint8_t * msg_buf = (uint8_t *) malloc(msg_size);
              uint8_t *msg_buf_ptr = msg_buf;

              std::set<LinkEvent *, SortLinkEvent>::iterator it = result->begin();
              std::set<LinkEvent *, SortLinkEvent>::iterator it_end = result->end();

              for (; it != it_end; it++) {
                LinkEvent *e = *it;
                LinkEvent *e_ = (LinkEvent *) msg_buf_ptr;
                e_->switch_id = e->switch_id;
                e_->link_id = e->link_id;
                e_->event_id = e->event_id;
                e_->state = e->state;
                //NS_LOG_LOGIC("[GossipReply] Server " << GetNode()->GetId() << " sends over switch " << e->switch_id << ", link " << e->link_id << ", event_id " << e->event_id );
                msg_buf_ptr += sizeof(LinkEvent);
              }

              Ptr<Packet> p = Create<Packet> (msg_buf, msg_size);
              if ((m_socket->SendPiloMessage(PiloHeader::ALL_NODES, GossipReply, p)) >= 0) {
                NS_LOG_INFO ("Sent gossip message to all controller nodes" );
              }
              
              delete result;
              free(buf);
              free(msg_buf);

            } else if (piloHdr.GetType() == GossipReply) {

              uint8_t *buf = (uint8_t *) malloc(packet->GetSize());
              packet->CopyData(buf, packet->GetSize());
              
              size_t total_size = 0;

              while (total_size < packet->GetSize()) {
                LinkEvent *e = (LinkEvent *) (buf + total_size);
                log->put_event(e->switch_id, e->link_id, e->event_id, e->state);
                
                //NS_LOG_LOGIC("[GossipReply Final] Server " << GetNode()->GetId() << " puts new event: switch " << e->switch_id << ", link " << e->link_id << ", event_id " << e->event_id );
                total_size += sizeof(LinkEvent);
              }

              free(buf);

            } else if (piloHdr.GetType() == LinkStateReply) {
              //NS_LOG_LOGIC("Server " << GetNode()->GetId() << " received LinkStateReply message from " << piloHdr.GetSourceNode());
              const size_t len = (const size_t) packet->GetSize();
              uint8_t buf[len];
              packet->CopyData(buf, packet->GetSize());
              
              // put the switch ID in local mapping: switch_id -> source node
              uint8_t *buf_ptr = buf;
              uint32_t *switch_id_ptr = (uint32_t *) buf_ptr;
              uint32_t switch_id = *switch_id_ptr;
              buf_ptr += sizeof(uint32_t);

              //Ipv4Address *addr = (Ipv4Address *) buf_ptr;
              //buf_ptr += sizeof(Ipv4Address);

              (*mapping)[*switch_id_ptr] = piloHdr.GetSourceNode();
              NS_LOG_LOGIC("Source node ID for switch " << *switch_id_ptr << " is " << piloHdr.GetSourceNode());
              
              int counter = 0;

              if (hosts->find(switch_id) == hosts->end()) {
                (*hosts)[switch_id] = new std::vector<uint32_t>();
              }
              (*hosts)[switch_id]->clear();

              while (sizeof(InterfaceStateMessage) * counter + sizeof(uint32_t) < len) {
                InterfaceStateMessage *m = (InterfaceStateMessage *) (buf_ptr + counter * sizeof(InterfaceStateMessage));
                if (m->is_host) {
                  (*hosts)[switch_id]->push_back(m->other_switch_id);
                  NS_LOG_LOGIC("switch_id: " << m->switch_id << " is connected to host " << m->other_switch_id);
                  
                } else {
                  log->put_event(m->switch_id, m->link_id, m->event_id, m->state);
                  NS_LOG_LOGIC("switch_id: " << m->switch_id << ", other switch id: " << m->other_switch_id << ", link id: " << m->link_id << ", event_id: " << m->event_id << ", state: " << m->state);

                }
                counter++;
              }

            } else {
              NS_LOG_LOGIC("Server " << GetNode()->GetId() << " received some other type " << piloHdr.GetType() <<
                           " from " << piloHdr.GetSourceNode());
            }
          }
      }
  }

  size_t PiloController::CtlGossipHelper(uint32_t switch_id, uint64_t link_id, uint8_t *buf) {

    ControllerState::LinkEventIterator event_it = log->link_event_begin();
    ControllerState::LinkEventIterator event_it_end = log->link_event_end();

    uint8_t *buf_ptr = buf;
    uint32_t *switch_id_ptr = (uint32_t *) buf_ptr;
    *switch_id_ptr = switch_id;
    buf_ptr += sizeof(switch_id);

    uint64_t *link_id_ptr = (uint64_t *) buf_ptr;
    *link_id_ptr = link_id;
    buf_ptr += sizeof(link_id);

    uint32_t *num_events = (uint32_t *) buf_ptr;
    *num_events = 0;
    buf_ptr += sizeof(uint32_t);

    uint64_t counter = 0;
    for (; event_it != event_it_end; event_it++) {     
      LinkEvent *e = *event_it;
      if (e->switch_id == switch_id && e->link_id == link_id) {
        counter = e->event_id;
        break;
      }
    }

    event_it = log->link_event_begin();
    for (; event_it != event_it_end; event_it++) {
      LinkEvent *e = *event_it;
      if (e->switch_id == switch_id && e->link_id == link_id) {
        // NS_LOG_LOGIC("Server " << GetNode()->GetId() << " link event: switch " << switch_id << ", link " << 
        //              link_id << ", event " << e->event_id);

        if (counter < e->event_id -1 && e->event_id > 0) {
          // there's a gap, put gap information in the buffer
          uint64_t *p = (uint64_t *) buf_ptr;
          *p = counter;
          buf_ptr += sizeof(counter);
          
          p = (uint64_t *) buf_ptr;
          *p = e->event_id;
          buf_ptr += sizeof(e->event_id);
          // NS_LOG_LOGIC("Server " << GetNode()->GetId() << " switch " << switch_id << ", link " << link_id <<
          //              " event gap: [" << counter << ", " << e->event_id << "]");
          *num_events = *num_events + 1;
        }
        counter = e->event_id;
      }
    }

    uint64_t *p = (uint64_t *) buf_ptr;
    *p = counter;
    buf_ptr += sizeof(counter);

    p = (uint64_t *) buf_ptr;
    *p = counter;
    buf_ptr += sizeof(counter);

    // NS_LOG_LOGIC("Server " << GetNode()->GetId() << " switch " << switch_id << ", link " << link_id <<
    //              " event gap: [" << counter << ", " << counter << "]");
    *num_events = *num_events + 1;
    // NS_LOG_LOGIC("Server " << GetNode()->GetId() << " switch " << switch_id << ", link " << link_id << " has " << 
    //              *num_events << " events");

    size_t total_size = (size_t) (buf_ptr - buf);
    return total_size;
  }

  void PiloController::CtlGossip(void) {
    // send over the gaps in events
    
    ControllerState::LinkIterator it = log->link_begin();
    ControllerState::LinkIterator it_end = log->link_end();

    //uint8_t *buf = (uint8_t *) malloc(sizeof(LinkEvent) * log->num_link_events()); 
    size_t per_link_size = (PiloController::gc + 1) * 2 * sizeof(uint64_t);
    size_t total_possible_size = (sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint32_t) + per_link_size) * log->num_links();
    uint8_t *buf = (uint8_t *) malloc(total_possible_size);
    //uint8_t *buf_ptr = buf;

    size_t total_actual_size = 0;

    for (; it != it_end; it++) {
      uint64_t link_id = *it;
      //uint64_t *link_id_ptr = (uint64_t *) buf_ptr;
      // *link_id_ptr = link_id;
      // buf_ptr += sizeof(link_id);

      // for each link, find the states for both switches
      uint32_t s0 = ControllerState::GetSwitch0(link_id);
      uint32_t s1 = ControllerState::GetSwitch1(link_id);
      
      total_actual_size += CtlGossipHelper(s0, link_id, buf+total_actual_size);
      total_actual_size += CtlGossipHelper(s1, link_id, buf+total_actual_size);
    }

    // send gossip message
    NS_ASSERT(total_possible_size >= total_actual_size);
    Ptr<Packet> p = Create<Packet> (buf, total_actual_size);
    if ((m_socket->SendPiloMessage(PiloHeader::ALL_NODES, GossipRequest, p)) >= 0) {
      NS_LOG_INFO ("Sent gossip message to all controller nodes" );
    }

    free(buf);
    
    // reschedule itself
    if (Simulator::Now().Compare(Seconds(final_time)) < 0) {
      Simulator::Schedule (Seconds(1), &PiloController::CtlGossip, this);
      gossip_send_counter++;
    }
        
  }

  void PiloController::GarbageCollect(void) {
    // this function is run periodically to garbage collect the log records to only keep the most recent X number of
    // link events for a given (switch, link_id) pair

    // iterate through all of the events and delete old log records
    // TODO: this is currently super inefficient

    log->reset_link_iterator();
    uint64_t link_id = 0;
    while (true) {
      if (!log->get_next_link(&link_id)) {
        break;
      }

      // switch0
      uint32_t s0 = ControllerState::GetSwitch0(link_id);
      ControllerState::LinkEventReverseIterator r_it = log->link_event_rbegin();
      ControllerState::LinkEventReverseIterator r_it_end = log->link_event_rend();
      int counter = 0;

      for (; r_it != r_it_end; r_it++) {
        LinkEvent *e = *r_it;
        if (e->switch_id == s0 && e->link_id == link_id) {
          ++counter;
          //NS_LOG_LOGIC("Server " << GetNode()->GetId() << " iterate switch: " << s0 << " event_id: " << e->event_id);
          if (counter == PiloController::gc) {
            break;
          }
        }
      }

      // delete the log entries from reverse iterator
      std::vector<LinkEvent *> deletes;
      for (; r_it != r_it_end; r_it++) {
        LinkEvent *e = *r_it;
        if (e->switch_id == s0 && e->link_id == link_id) {
          // NS_LOG_LOGIC("Server " << GetNode()->GetId() << " deleting " << (*r_it)->switch_id << ", " << (*r_it)->link_id << ", " <<
          //              (*r_it)->event_id);
          deletes.push_back(e);
        }
      }

      for (size_t i = 0; i < deletes.size(); i++) {
        LinkEvent *e = deletes[i];
        log->delete_link_event(e->switch_id, e->link_id, e->event_id, e->state);
      }

      // switch1
      uint32_t s1 = ControllerState::GetSwitch1(link_id);
      r_it = log->link_event_rbegin();
      r_it_end = log->link_event_rend();
      counter = 0;

      for (; r_it != r_it_end; r_it++) {
        LinkEvent *e = *r_it;
        if (e->switch_id == s1 && e->link_id == link_id) {
          ++counter;
          //NS_LOG_LOGIC("Server " << GetNode()->GetId() << " iterate switch: " << s1 << " event_id: " << e->event_id);
          if (counter == PiloController::gc) {
            break;
          }
        }
      }

      // delete the log entries from reverse iterator
      deletes.clear();
      for (; r_it != r_it_end; r_it++) {
        LinkEvent *e = *r_it;
        if (e->switch_id == s1 && e->link_id == link_id) {
          // NS_LOG_LOGIC("Server " << GetNode()->GetId() << " deleting " << (*r_it)->switch_id << ", " << (*r_it)->link_id << ", " <<
          //              (*r_it)->event_id);
          deletes.push_back(e);
        }
      }

      for (size_t i = 0; i < deletes.size(); i++) {
        LinkEvent *e = deletes[i];
        log->delete_link_event(e->switch_id, e->link_id, e->event_id, e->state);
      }

    }

    // // DEBUG CHECK
    // CurrentLog();

    // reschedule itself
    if (Simulator::Now().Compare(Seconds(final_time)) < 0) {
      Simulator::Schedule (Seconds(1), &PiloController::GarbageCollect, this);
      gossip_send_counter++;
    }
  }

  void PiloController::AssignRoutes() {
    // for all switches, construct graph from the most "recent" link state events
    std::set<uint32_t> *switches = new std::set<uint32_t>();
    std::map<uint64_t, bool> *connection_graph = new std::map<uint64_t, bool>();

    ControllerState::LinkIterator l_it = log->link_begin();
    ControllerState::LinkIterator l_it_end = log->link_end();

    ControllerState::LinkEventIterator le_it = log->link_event_begin();
    ControllerState::LinkEventIterator le_it_end = log->link_event_end();

    for (; l_it != l_it_end; l_it++) {
      uint64_t link = *l_it;
      le_it = log->link_event_begin();

      uint32_t switch0 = ControllerState::GetSwitch0(link);
      uint32_t switch1 = ControllerState::GetSwitch1(link);

      switches->insert(switch0);
      switches->insert(switch1);
      
      uint64_t highest_event_id = 0;
      bool state = true;
      bool found = false;
      
      // try for the highest event id per link
      for (; le_it != le_it_end; le_it++) {
        LinkEvent *e = *le_it;
        if (e->link_id == link) {
          if (e->event_id >= highest_event_id) {
            found = true;
            highest_event_id = e->event_id;
            state = e->state;
          }
        }
      }
      
      if (found) {
        (*connection_graph)[link] = state;
        NS_LOG_LOGIC("Link event found for link id " << link << " between switches " << switch0 << " and " << switch1
                     << ", state is " << state);
      }
    }

    // // process the network connection graph and find shortest path routing
    size_t num_switches = switches->size();
    Graph g(num_switches);
    std::map<uint64_t, bool>::iterator it = connection_graph->begin();
    std::map<uint64_t, bool>::iterator it_end = connection_graph->end();

    NS_LOG_LOGIC("Constructing graph from log, connection graph has " << connection_graph->size() << " entries");
    for (; it != it_end; it++) {
      uint64_t link_id = it->first;
      uint32_t switch0 = ControllerState::GetSwitch0(link_id);
      uint32_t switch1 = ControllerState::GetSwitch1(link_id);

      if (it->second) {
        add_edge(switch0, switch1, 1, g);
        add_edge(switch1, switch0, 1, g);
        NS_LOG_LOGIC("Adding edge between " << switch0 << " and " << switch1);
      }
      NS_LOG_LOGIC("Edge between " << switch0 << " and " << switch1 << " state is " << it->second);
    }

    // put the hosts' locations in graph
    std::map<uint32_t, std::vector<uint32_t> *>::iterator h_it = hosts->begin();
    std::map<uint32_t, std::vector<uint32_t> *>::iterator h_it_end = hosts->end();

    std::map<uint32_t, uint32_t> *host_ids = new std::map<uint32_t, uint32_t>();
    uint32_t counter = (uint32_t) switches->size();

    for (; h_it != h_it_end; h_it++) {
      uint32_t s = h_it->first;
      std::vector<uint32_t> *host_list = h_it->second;
      
      for (size_t i = 0; i < host_list->size(); i++) {
        (*host_ids)[counter] = host_list->at(i);
        NS_LOG_LOGIC("Begin adding edge between " << s << " and " << host_list->at(i));
        add_edge(s, counter, 1, g);
        add_edge(counter, s, 1, g);
        NS_LOG_LOGIC("End adding edge between " << s << " and " << host_list->at(i));
        ++counter;
      }
    }

    //property_map<Graph, edge_weight_t>::type weightmap = get(edge_weight, g);
    std::set<uint32_t>::iterator s_it = switches->begin();
    std::set<uint32_t>::iterator s_it_end = switches->end();

    std::map<uint32_t, uint32_t>::iterator h_id_it = host_ids->begin();
    std::map<uint32_t, uint32_t>::iterator h_id_it_end = host_ids->end();

    h_it = hosts->begin();
    for (; h_id_it != h_id_it_end; h_id_it++) {
      uint32_t host_id = h_id_it->first;
      uint32_t host_addr = h_id_it->second;

      vertex_descriptor vs = vertex(host_id, g);
      std::vector<vertex_descriptor> p(num_vertices(g));
      std::vector<int> d(num_vertices(g));

      graph_traits < Graph >::vertex_iterator vi, vend;
      NS_LOG_LOGIC("Running Dijkstra on host " << host_id);
      
      dijkstra_shortest_paths(g, vs,
                              predecessor_map(boost::make_iterator_property_map(p.begin(), get(boost::vertex_index, g))).
                              distance_map(boost::make_iterator_property_map(d.begin(), get(boost::vertex_index, g))));

      for (boost::tie(vi, vend) = vertices(g); vi != vend; ++vi) {
        NS_LOG_LOGIC("parent(" << *vi << ") = " << p[*vi]);

        // message mapping: destination IP --> routing switch ID
        if (host_id == *vi) {
          continue;
        }
         
        uint32_t idx = *vi;
        uint32_t last_node = p[idx];
        // while (true) {
        //   if (p[idx] == host_id || p[idx] == idx) {
        //     break;
        //   }

        //   last_node = idx;
        //   idx = p[idx];
        // }
        NS_LOG_LOGIC("Destination " << host_addr << " Needs routing from " << last_node);

        // TODO: make this step more efficient
        uint8_t *buf = (uint8_t *) malloc(8);

        uint32_t *dest_addr = (uint32_t *) buf;
        *dest_addr = host_addr;
        uint32_t * switch_ptr = (uint32_t *) (buf + sizeof(uint32_t));
        *switch_ptr = last_node;
        uint32_t switch_node = (*mapping)[*vi];

        Ptr<Packet> p = Create<Packet> (buf, 8);
        if ((m_socket->SendPiloMessage(switch_node, AddRoute, p)) >= 0) {
          NS_LOG_INFO ("Sent AddRoute message to switch " << *vi << " with node id " << switch_node << " for address " << *dest_addr);
        }
        
      }
    }

    delete switches;
    delete connection_graph;
    delete host_ids;
    
    if (Simulator::Now().Compare(Seconds(final_time)) < 0) {
      Simulator::Schedule (Seconds(1), &PiloController::AssignRoutes, this);
      gossip_send_counter++;
    }

  }

  // DEBUG 
  void PiloController::CurrentLog(void) {
    log->reset_event_iterator();

    NS_LOG_LOGIC("Server " << GetNode()->GetId() << " current log: ");
    while (true) {
      LinkEvent *e = log->get_next_event();
      if (e == NULL) 
        break;

      NS_LOG_LOGIC("link_event: [" << e->switch_id << ", " << e->link_id << ", " << e->event_id << ", " << e->state << "]");
    }
  }

} // Namespace ns3
