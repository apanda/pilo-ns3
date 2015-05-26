/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
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
 * Author: Mohamed Amine Ismail <amine.ismail@sophia.inria.fr>
 */
#ifndef PILO_CTL_HELPER_H
#define PILO_CTL_HELPER_H

#include <stdint.h>
#include "ns3/application-container.h"
#include "ns3/node-container.h"
#include "ns3/object-factory.h"
#include "ns3/ipv4-address.h"
#include "ns3/pilo-ctl-server.h"
#include "ns3/pilo-ctl-client.h"
namespace ns3 {
/**
 * \ingroup udpclientserver
 * \brief Create a server application which waits for input UDP packets
 *        and uses the information carried into their payload to compute
 *        delay and to determine if some packets are lost.
 */
class PiloCtlServerHelper
{
public:
  /**
   * Create PiloCtlServerHelper which will make life easier for people trying
   * to set up simulations with udp-client-server application.
   *
   */
  PiloCtlServerHelper ();

  /**
   * Create PiloCtlServerHelper which will make life easier for people trying
   * to set up simulations with udp-client-server application.
   *
   * \param port The port the server will wait on for incoming packets
   */
  PiloCtlServerHelper (uint16_t port);

  /**
   * Record an attribute to be set in each Application after it is is created.
   *
   * \param name the name of the attribute to set
   * \param value the value of the attribute to set
   */
  void SetAttribute (std::string name, const AttributeValue &value);

  /**
   * Create one UDP server application on each of the Nodes in the
   * NodeContainer.
   *
   * \param c The nodes on which to create the Applications.  The nodes
   *          are specified by a NodeContainer.
   * \returns The applications created, one Application per Node in the
   *          NodeContainer.
   */
  ApplicationContainer Install (NodeContainer c);

  /**
   * \brief Return the last created server.
   *
   * This function is mainly used for testing.
   *
   * \returns a Ptr to the last created server application
   */
  Ptr<PiloCtlServer> GetServer (void);
private:
  ObjectFactory m_factory; //!< Object factory.
  Ptr<PiloCtlServer> m_server; //!< The last created server application
};

class PiloCtlClientHelper
{

public:
  /**
   * Create PiloCtlClientHelper which will make life easier for people trying
   * to set up simulations with udp-client-server.
   *
   */
  PiloCtlClientHelper ();

  /**
   *  Create PiloCtlClientHelper which will make life easier for people trying
   * to set up simulations with udp-client-server.
   *
   * \param ip The IPv4 address of the remote UDP server
   * \param port The port number of the remote UDP server
   */

  PiloCtlClientHelper (Ipv4Address ip, uint16_t port);
  /**
   *  Create PiloCtlClientHelper which will make life easier for people trying
   * to set up simulations with udp-client-server.
   *
   * \param ip The IPv6 address of the remote UDP server
   * \param port The port number of the remote UDP server
   */

  PiloCtlClientHelper (Ipv6Address ip, uint16_t port);
  /**
   *  Create PiloCtlClientHelper which will make life easier for people trying
   * to set up simulations with udp-client-server.
   *
   * \param ip The IP address of the remote UDP server
   * \param port The port number of the remote UDP server
   */

  PiloCtlClientHelper (Address ip, uint16_t port);

  /**
   * Record an attribute to be set in each Application after it is is created.
   *
   * \param name the name of the attribute to set
   * \param value the value of the attribute to set
   */
  void SetAttribute (std::string name, const AttributeValue &value);

  /**
     * \param c the nodes
     *
     * Create one UDP client application on each of the input nodes
     *
     * \returns the applications created, one application per input node.
     */
  ApplicationContainer Install (NodeContainer c);

private:
  ObjectFactory m_factory; //!< Object factory.
};

} // namespace ns3

#endif /* PILO_CTL_H */
