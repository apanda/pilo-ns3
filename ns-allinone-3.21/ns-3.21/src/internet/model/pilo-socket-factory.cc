/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * PILO code PILO code PILO code
 */
#include "pilo-socket-factory.h"
#include "ipv4-l3-protocol.h"
#include "ns3/socket.h"
#include "ns3/uinteger.h"
#include "ns3/log.h"

NS_LOG_COMPONENT_DEFINE ("PiloSocketFactory");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (PiloSocketFactory);

TypeId PiloSocketFactory::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PiloSocketFactory")
    .SetParent<SocketFactory> ()
  ;
  return tid;
}

Ptr<Socket> 
PiloSocketFactory::CreateSocket (void)
{
  NS_LOG_FUNCTION (this);
  Ptr<Ipv4> ipv4 = GetObject<Ipv4> ();
  Ptr<Socket> socket = ipv4->CreatePiloCtlSocket ();
  return socket;
}

}
