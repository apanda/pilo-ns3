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
  if (link_state_send_counter < max_counter * 13) {
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
              // find the difference between the two sets
              ControllerState *copy = new ControllerState();
              const size_t size = (const size_t) packet->GetSize();
              if (size == 0) {
                return;
              }

              uint8_t buf[size];
              packet->CopyData(buf, packet->GetSize());
              
              NS_LOG_LOGIC("Server " << GetNode()->GetId() << " receives GossipRequest");

              int counter = 0;
              while (counter * sizeof(LinkEvent) < packet->GetSize()) {
                LinkEvent *e = (LinkEvent *) (buf + counter * sizeof(LinkEvent));

                NS_LOG_LOGIC("switch_id: " << e->switch_id  << ", link id: " << e->link_id << ", event_id: " << e->event_id << ", state: " << e->state);

                copy->put_event(e->switch_id, e->link_id, e->event_id, e->state);
                if (!log->event_in_log(e->switch_id, e->link_id, e->event_id, e->state)) {
                  log->put_event(e->switch_id, e->link_id, e->event_id, e->state);
                }
                counter++;
              }

              if (log->num_link_events() == 0) {
                return;
              }
              
              counter = 0;

              uint8_t *ret_buf = (uint8_t *) malloc(sizeof(LinkEvent) * log->num_link_events());

              log->reset_event_iterator();
              while (true) {
                LinkEvent *e = log->get_next_event();
                if (e == NULL) {
                  break;
                }

                if (!copy->event_in_log(e->switch_id, e->link_id, e->event_id, e->state)) {
                  LinkEvent *e_ = (LinkEvent *) (ret_buf + counter * sizeof(LinkEvent));
                  e_->switch_id = e->switch_id;
                  e_->link_id = e->link_id;
                  e_->event_id = e->event_id;
                  e_->state = e->state;
                  counter++;
                }
              }

              Ptr<Packet> p = Create<Packet> (ret_buf, counter * sizeof(LinkEvent));
              if ((m_socket->SendPiloMessage(PiloHeader::ALL_NODES, GossipReply, p)) >= 0) {
                NS_LOG_INFO ("Sent gossip message to all controller nodes" );
              }

              delete copy;
              delete ret_buf;

            } else if (piloHdr.GetType() == GossipReply) {

              const size_t size = (const size_t) packet->GetSize();
              uint8_t buf[size];
              packet->CopyData(buf, packet->GetSize());
              
              int counter = 0;
              while (counter * sizeof(LinkEvent) < packet->GetSize()) {
                LinkEvent *e = (LinkEvent *) (buf + counter * sizeof(LinkEvent));
                if (!log->event_in_log(e->switch_id, e->link_id, e->event_id, e->state)) {
                  log->put_event(e->switch_id, e->link_id, e->event_id, e->state);
                }
                counter++;
              }

            } else if (piloHdr.GetType() == LinkStateReply) {
              //NS_LOG_LOGIC("Server " << GetNode()->GetId() << " received LinkStateReply message from " << piloHdr.GetSourceNode());
              const size_t len = (const size_t) packet->GetSize();
              uint8_t buf[len];
              packet->CopyData(buf, packet->GetSize());
              int counter = 0;

              while (sizeof(InterfaceStateMessage) * counter < len) {
                InterfaceStateMessage *m = (InterfaceStateMessage *) (buf + counter * sizeof(InterfaceStateMessage));
                log->put_event(m->switch_id, m->link_id, m->event_id, m->state);
                // NS_LOG_LOGIC("switch_id: " << m->switch_id << ", other switch id: " << m->other_switch_id << ", link id: " << m->link_id << ", event_id: " << m->event_id << ", state: " << m->state);
                counter++;
              }

            } else {
              NS_LOG_LOGIC("Server " << GetNode()->GetId() << " received some other type " << piloHdr.GetType() <<
                           " from " << piloHdr.GetSourceNode());
            }
          }
      }
  }

  void PiloController::CtlGossip(void) {
    // send over a list of existing link events

    NS_LOG_INFO("Server " << GetNode()->GetId() << " gossip request scheduled");
    const size_t total_size = (const size_t) log->num_link_events() * sizeof(LinkEvent);
    uint8_t buf[total_size];
    int counter = 0;

    log->reset_event_iterator();

    while (true) {
      LinkEvent *e_ = log->get_next_event();
      if (e_ == NULL)
        break;

      LinkEvent *e = (LinkEvent *) (buf + counter * sizeof(LinkEvent));
      e->switch_id = e_->switch_id;
      e->link_id = e_->link_id;
      e->event_id = e_->event_id;
      e->state = e_->state;
      counter++;
    }

    Ptr<Packet> p = Create<Packet> (buf, total_size);
    if ((m_socket->SendPiloMessage(PiloHeader::ALL_NODES, GossipRequest, p)) >= 0) {
      NS_LOG_INFO ("Sent gossip message to all controller nodes" );
    }

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
          NS_LOG_LOGIC("Server " << GetNode()->GetId() << " iterate switch: " << s0 << " event_id: " << e->event_id);
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
          NS_LOG_LOGIC("Server " << GetNode()->GetId() << " deleting " << (*r_it)->switch_id << ", " << (*r_it)->link_id << ", " <<
                       (*r_it)->event_id);
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
          NS_LOG_LOGIC("Server " << GetNode()->GetId() << " iterate switch: " << s1 << " event_id: " << e->event_id);
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
          NS_LOG_LOGIC("Server " << GetNode()->GetId() << " deleting " << (*r_it)->switch_id << ", " << (*r_it)->link_id << ", " <<
                       (*r_it)->event_id);
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
