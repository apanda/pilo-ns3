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

namespace ns3 {

  class LinkEvent {
    
  public:
    //
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
  std::set<LinkEvent *> *link_states;
};

} // namespace ns3

#endif /* PILO_CTLCLIENT_H */
