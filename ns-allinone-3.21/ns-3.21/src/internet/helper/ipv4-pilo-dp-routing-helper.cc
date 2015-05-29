/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * PILO code PILO code PILO code
 */
#include "ipv4-pilo-dp-routing-helper.h"
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

Ptr<Ipv4PiloDPRouting>
Ipv4PiloDPRoutingHelper::GetPiloDPRouting (Ptr<Ipv4> ipv4) const
{
  //NS_LOG_FUNCTION (this);
  Ptr<Ipv4RoutingProtocol> ipv4rp = ipv4->GetRoutingProtocol ();
  NS_ASSERT_MSG (ipv4rp, "No routing protocol associated with Ipv4");
  if (DynamicCast<Ipv4PiloDPRouting> (ipv4rp))
    {
      //NS_LOG_LOGIC ("Static routing found as the main IPv4 routing protocol.");
      return DynamicCast<Ipv4PiloDPRouting> (ipv4rp); 
    } 
  if (DynamicCast<Ipv4ListRouting> (ipv4rp))
    {
      Ptr<Ipv4ListRouting> lrp = DynamicCast<Ipv4ListRouting> (ipv4rp);
      int16_t priority;
      for (uint32_t i = 0; i < lrp->GetNRoutingProtocols ();  i++)
        {
          //NS_LOG_LOGIC ("Searching for static routing in list");
          Ptr<Ipv4RoutingProtocol> temp = lrp->GetRoutingProtocol (i, priority);
          if (DynamicCast<Ipv4PiloDPRouting> (temp))
            {
              //NS_LOG_LOGIC ("Found static routing in list");
              return DynamicCast<Ipv4PiloDPRouting> (temp);
            }
        }
    }
  //NS_LOG_LOGIC ("Pilo DP routing not found");
  return 0;
}

}
