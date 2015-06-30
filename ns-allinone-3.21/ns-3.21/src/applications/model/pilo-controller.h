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
 *
 */

#ifndef PILO_CONTROLLER_H
#define PILO_CONTROLLER_H

#include <map>
#include <set>

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-address.h"
#include "ns3/pilo-socket.h"
#include "ns3/internet-module.h"

#include <boost/config.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/property_map/property_map.hpp>

namespace ns3 {
  using namespace boost;

  typedef adjacency_list<vecS, vecS, bidirectionalS, no_property, property<edge_weight_t, int> > Graph;
  typedef graph_traits <Graph>::vertex_descriptor vertex_descriptor;
  typedef std::pair<uint32_t, uint32_t> Edge;
  typedef Graph::edge_descriptor edge_descriptor;

  class AddRouteMessage {
  public:
    AddRouteMessage() {
      message = new std::map<uint32_t, uint32_t>();
    }

    ~AddRouteMessage() {
      delete message;
    }
    
    int copy_buf(uint8_t *buf, int max_size) {
      // copy all events into buf
      size_t ret_size = sizeof(uint32_t) * 2 * message->size();
      if (ret_size > (size_t) max_size) {
        return -1;
      } else {

        std::map<uint32_t, uint32_t>::iterator it = message->begin();
        std::map<uint32_t, uint32_t>::iterator it_end = message->end();
        int count = 0;
        
        for (; it != it_end; it++) {
          uint32_t dest_addr = it->first;
          uint32_t switch_node = it->second;
          
          memcpy(buf + count * sizeof(uint32_t), (uint8_t *) &dest_addr, sizeof(uint32_t));
          ++count;
          memcpy(buf + count * sizeof(uint32_t), (uint8_t *) &switch_node, sizeof(uint32_t));
          ++count;
        }

        return (int) ret_size;
      }
    }

    void add_entry(uint32_t dest, uint32_t switch_node) {
      (*message)[dest] = switch_node;
    }

    std::map<uint32_t, uint32_t> *message;
  };
  
  class LinkEvent {
    
  public:
    LinkEvent(uint32_t switch_id, uint64_t link_id, uint64_t event_id, bool state)
    {
      this->switch_id = switch_id;
      this->link_id = link_id;
      this->event_id = event_id;
      this->state = state;
    }

    bool compare(const LinkEvent *e) const {
      if (this->switch_id != e->switch_id)
        return this->switch_id < e->switch_id;
      
      if (this->link_id != e->link_id)
        return this->link_id < e->link_id;
      
      return this->event_id < e->event_id;
    }

    uint32_t switch_id;  // 4 bytes
    uint64_t link_id;    // 8 bytes
    uint64_t event_id;   // 8 bytes
    bool state;          // 1 byte
  };

  struct SortLinkEvent {
    bool operator()(const LinkEvent *a, const LinkEvent *b) {
      return a->compare(b);
    }
  };

  // this class contains the current state of the controller
  // this is basically a log of all of the events received by the controller
  class ControllerState {
  public:
    typedef std::set<uint64_t>::iterator LinkIterator;
    typedef std::set<uint64_t>::reverse_iterator LinkReverseIterator;

    typedef std::set<LinkEvent *, SortLinkEvent>::iterator LinkEventIterator;
    typedef std::set<LinkEvent *, SortLinkEvent>::reverse_iterator LinkEventReverseIterator;

    ControllerState() {
      log = new std::set<LinkEvent *, SortLinkEvent>();
      it = log->begin();
      links = new std::set<uint64_t>();
    }

    ~ControllerState() {
      std::set<LinkEvent *, SortLinkEvent>::iterator it = log->begin();
      std::set<LinkEvent *, SortLinkEvent>::iterator it_end = log->end();

      for (; it != it_end; it++) {
        delete (*it);
      }

      delete log;
      delete links;
    }

    // returns the first gap in log
    bool get_event_gap(uint32_t switch_id, uint64_t link_id,
                       uint64_t *low_event_id, uint64_t *high_event_id) {
      // iterate over all of the logs
      std::set<LinkEvent *, SortLinkEvent>::iterator it = log->begin();
      std::set<LinkEvent *, SortLinkEvent>::iterator it_end = log->end();

      uint64_t last_event_id = (*it)->event_id;
      bool gap_found = false;

      for (; it != it_end; it++) {
        LinkEvent *e = *it;
        if (e->switch_id == switch_id && e->link_id == link_id) {
          if (last_event_id < e->event_id - 1 && last_event_id != e->event_id) {
            *low_event_id = last_event_id;
            *high_event_id = e->event_id;
            gap_found = true;
            break;
          } else {
            last_event_id = e->event_id;
          }
        }
      }

      return gap_found;
    }

    // get all the events within (low_event_id, high_event_id)
    void get_events_within_gap(uint32_t switch_id, uint64_t link_id, 
                               uint64_t low_event_id, uint64_t high_event_id,
                               std::set<LinkEvent *, SortLinkEvent> *result) {
      std::set<LinkEvent *, SortLinkEvent>::iterator it = log->begin();
      std::set<LinkEvent *, SortLinkEvent>::iterator it_end = log->end();

      for (; it != it_end; it++) {
        LinkEvent *e = *it;
        if (e->switch_id == switch_id && e->link_id == link_id && 
            e->event_id > low_event_id && e->event_id < high_event_id) {
          //(*result)[e->event_id] = e->state;
          result->insert(e);
        }
      }      
    }

    void get_greater_events(uint32_t switch_id, uint64_t link_id, uint64_t event_id,
                            std::set<LinkEvent *, SortLinkEvent> *result) {

      std::set<LinkEvent *, SortLinkEvent>::iterator it = log->begin();
      std::set<LinkEvent *, SortLinkEvent>::iterator it_end = log->end();

      for (; it != it_end; it++) {
        LinkEvent *e = *it;
        if (e->switch_id == switch_id && e->link_id == link_id && e->event_id > event_id) {
          //(*result)[e->event_id] = e->state;
          result->insert(e);
        }
      }      
    }

    void get_smaller_events(uint32_t switch_id, uint64_t link_id, uint64_t event_id,
                            std::set<LinkEvent *, SortLinkEvent> *result) {

      std::set<LinkEvent *, SortLinkEvent>::iterator it = log->begin();
      std::set<LinkEvent *, SortLinkEvent>::iterator it_end = log->end();

      for (; it != it_end; it++) {
        LinkEvent *e = *it;
        if (e->switch_id == switch_id && e->link_id == link_id && e->event_id < event_id) {
          //(*result)[e->event_id] = e->state;
          result->insert(e);
        }
      }      
    }

    void put_event(uint32_t switch_id, uint64_t link_id, uint64_t event_id, bool state) {
      LinkEvent *e = new LinkEvent(switch_id, link_id, event_id, state);
      log->insert(e);
      links->insert(e->link_id);
    }

    void reset_event_iterator() {
      it = log->begin();
    }

    void reset_link_iterator() {
      it_link = links->begin();
    }

    LinkEvent* get_next_event() {
      if (it == log->end())
        return NULL;
      LinkEvent *e = *it;
      it++;
      return e;
    }

    bool get_next_link(uint64_t *ret) {
      if (it_link == links->end())
        return false;
      *ret = *it_link;
      it_link++;
      return true;
    }

    static uint32_t GetSwitch0(uint64_t link_id) {
      uint64_t link_ = link_id;
      uint32_t *ptr0 = (uint32_t *) &link_;
      return *ptr0;
    }

    static uint32_t GetSwitch1(uint64_t link_id) {
      uint64_t link_ = link_id;
      uint32_t *ptr1 = ((uint32_t *) &link_)+1;
      return *ptr1;
    }

    size_t num_link_events() {
      return log->size();
    }

    size_t num_links() {
      return links->size();
    }

    bool event_in_log(LinkEvent *e) {
      return (log->find(e) != log->end());
    }

    bool event_in_log(uint32_t switch_id, uint64_t link_id, uint64_t event_id, bool state) {
      std::set<LinkEvent *, SortLinkEvent>::iterator it_start_ = log->begin();
      std::set<LinkEvent *, SortLinkEvent>::iterator it_end_ = log->end();

      for (; it_start_ != it_end_; it_start_++) {
        if ((*it_start_)->switch_id == switch_id && 
            (*it_start_)->link_id == link_id && 
            (*it_start_)->event_id == event_id) {
          NS_ASSERT((*it_start_)->state == state);
          return true;
        }
      }

      return false;
    }

    LinkEventIterator link_event_begin() {
      return log->begin();
    }

    LinkEventIterator link_event_end() {
      return log->end();
    }

    LinkEventReverseIterator link_event_rbegin() {
      return log->rbegin();
    }

    LinkEventReverseIterator link_event_rend() {
      return log->rend();
    }

    void delete_link_event(LinkEventReverseIterator r_it) {
      LinkEvent *e = *(r_it.base());
      log->erase(r_it.base());
      delete e;
    }

    void delete_link_event(LinkEventIterator it) {
      LinkEvent *e = *it;
      log->erase(it);
      delete e;
    }

    void delete_link_event(uint32_t switch_id, uint64_t link_id, uint64_t event_id, bool state) {
      std::set<LinkEvent *, SortLinkEvent>::iterator it_start_ = log->begin();
      std::set<LinkEvent *, SortLinkEvent>::iterator it_end_ = log->end();

      for (; it_start_ != it_end_; it_start_++) {
        if ((*it_start_)->switch_id == switch_id && 
            (*it_start_)->link_id == link_id && 
            (*it_start_)->event_id == event_id) {
     
          LinkEvent *e = *it_start_;
          log->erase(it_start_);
          delete e;
          return;
        }
      }

    }

    LinkIterator link_begin() {
      return links->begin();
    }

    LinkIterator link_end() {
      return links->end();
    }

    std::set<LinkEvent *, SortLinkEvent> *log;
    std::set<LinkEvent *, SortLinkEvent>::iterator it;
    std::set<uint64_t>::iterator it_link;

    std::set<uint64_t> *links; // a list of link ids
  };

  struct pilo_gossip_request {
    uint32_t switch_id;
    uint64_t link_id;
    uint64_t low_event_id;
    uint64_t high_event_id;
  };

  struct pilo_gossip_reply_header {
    uint32_t switch_id;
    uint64_t link_id;
  };

  struct pilo_gossip_reply_single {
    uint64_t event_id;
    bool state;
  };
  
class Socket;
class Packet;

/**
 * \ingroup udpclientserver
 * \class PiloController
 * \brief A PiloCtl client. Sends UDP packet carrying sequence number and time stamp
 *  in their payloads
 *
 */
class PiloController : public Application
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  PiloController ();

  virtual ~PiloController ();

  /**
   * \brief set the remote address and port
   * \param ip remote IPv4 address
   * \param port remote port
   */
  void SetRemote (Ipv4Address ip, uint16_t port);
  /**
   * \brief set the remote address and port
   * \param ip remote IPv6 address
   * \param port remote port
   */
  void SetRemote (Ipv6Address ip, uint16_t port);
  /**
   * \brief set the remote address and port
   * \param ip remote IP address
   * \param port remote port
   */
  void SetRemote (Address ip, uint16_t port);

  void CtlGossip(void);
  void GetLinkState(void);
  void CurrentLog(void);
  void GarbageCollect(void);
  size_t CtlGossipHelper(uint32_t switch_id, uint64_t link_id, uint8_t *buf);
  void AssignRoutes();

  void GetBandwidthInfo() {
    double bw = (double) total_bytes  * 8 / 1000.0 / Simulator::Now().GetSeconds();
    printf("[bw][Controller] bandwidth: %f Mb/s\n", bw);
  }

  void AddBandwidth(uint64_t bytes) {
    total_bytes += bytes;
  }

protected:
  virtual void DoDispose (void);

private:

  virtual void StartApplication (void);
  virtual void StopApplication (void);
  void HandleRead (Ptr<Socket> socket);
  /**
   * \brief Send a packet
   */
  void Send (void);

  uint32_t m_count; //!< Maximum number of packets the application will send
  Time m_interval; //!< Packet inter-send time
  uint32_t m_size; //!< Size of the sent packet (including the SeqTsHeader)
  uint32_t m_targetNode; //!< Node to send to

  uint32_t m_sent; //!< Counter for sent packets
  Ptr<PiloSocket> m_socket; //!< Socket
  Address m_peerAddress; //!< Remote peer address
  uint16_t m_peerPort; //!< Remote peer port
  EventId m_sendEvent; //!< Event to send the next packet

  // <packet uid, source>
  std::map<uint32_t, uint32_t> *messages;
  ControllerState *log;
  std::map<uint32_t, uint32_t> *mapping;
  std::map<uint32_t, std::vector<uint32_t> *> *hosts; // switch-to-host mapping
  int gossip_send_counter;
  int link_state_send_counter;
  int max_counter;

  std::map<uint64_t, uint64_t> *bandwidth_per_link;
  uint64_t total_bytes;

  static const int gc = 5;
  static const int final_time = 100;
};

} // namespace ns3

#endif /* PILO_CTLCLIENT_H */
