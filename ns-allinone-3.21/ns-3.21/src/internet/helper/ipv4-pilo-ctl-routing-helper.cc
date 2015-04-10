/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * PILO code PILO code PILO code
 */
#include "ipv4-pilo-ctl-routing-helper.h"
#include "ns3/ipv4-pilo-ctl-routing.h"
#include "ns3/node.h"

namespace ns3 {

Ipv4PiloCtlRoutingHelper* 
Ipv4PiloCtlRoutingHelper::Copy (void) const 
{
  return new Ipv4PiloCtlRoutingHelper (*this); 
}

Ptr<Ipv4RoutingProtocol> 
Ipv4PiloCtlRoutingHelper::Create (Ptr<Node> node) const
{
  return CreateObject<Ipv4PiloCtlRouting> ();
}
}
