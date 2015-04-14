/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * PILO code PILO code PILO code
 */
#include "ipv4-pilo-dp-routing-helper.h"
#include "ns3/ipv4-pilo-dp-routing.h"
#include "ns3/node.h"

namespace ns3 {

Ipv4PiloDPRoutingHelper* 
Ipv4PiloDPRoutingHelper::Copy (void) const 
{
  return new Ipv4PiloDPRoutingHelper (*this); 
}

Ptr<Ipv4RoutingProtocol> 
Ipv4PiloDPRoutingHelper::Create (Ptr<Node> node) const
{
  return CreateObject<Ipv4PiloDPRouting> ();
}
}
