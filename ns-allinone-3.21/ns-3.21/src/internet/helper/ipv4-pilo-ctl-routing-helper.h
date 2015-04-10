/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * PILO code PILO code PILO code
 */

#ifndef IPV4_PILO_CTL_ROUTING_HELPER_H
#define IPV4_PILO_CTL_ROUTING_HELPER_H
#include "ns3/ipv4-routing-helper.h"
namespace ns3 {
class Ipv4PiloCtlRoutingHelper : public Ipv4RoutingHelper {
public:
  // Constructor
  Ipv4PiloCtlRoutingHelper () {};

  /**
   * \brief virtual constructor
   * \returns pointer to clone of this Ipv4RoutingHelper 
   * 
   * This method is mainly for internal use by the other helpers;
   * clients are expected to free the dynamic memory allocated by this method
   */
  virtual Ipv4PiloCtlRoutingHelper* Copy (void) const;

  /**
   * \param node the node within which the new routing protocol will run
   * \returns a newly-created routing protocol
   */
  virtual Ptr<Ipv4RoutingProtocol> Create (Ptr<Node> node) const;

};
}
#endif
