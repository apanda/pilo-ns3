/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * PILO code PILO code PILO code
 */
#include "ipv4-raw-socket-impl.h"
#include "ipv4-l3-protocol.h"
#include "ns3/ipv4-packet-info-tag.h"
#include "ns3/inet-socket-address.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"
#include "ns3/log.h"
#include "pilo-socket.h"
NS_LOG_COMPONENT_DEFINE ("PiloSocket");
namespace ns3 {

  uint64_t rand_64_num() {
    static uint64_t x=123456789, y=362436069, z=521288629;
    uint64_t t;
    x ^= x << 16;
    x ^= x >> 5;
    x ^= x << 1;
    t = x;
    x = y;
    y = z;
    z = t ^ x ^ y;
    return z;
  } 

NS_OBJECT_ENSURE_REGISTERED (PiloSocket);

TypeId 
PiloSocket::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PiloSocket")
    .SetParent<Ipv4RawSocketImpl> ();
  return tid;
}

PiloSocket::PiloSocket () {
  // A protocol to get all the PILO goodness
  SetProtocol(PROTOCOL);
  m_iphdrRecv = false;
}

// @apanda: Mostly this is just overriding stuff in Ipv4 raw socket.
int 
PiloSocket::SendTo (Ptr<Packet> p, uint32_t flags, 
                           const Address &toAddress)
{
  NS_LOG_FUNCTION (this << p << flags << toAddress);
  NS_ASSERT(!m_iphdrincl);
  if (!InetSocketAddress::IsMatchingType (toAddress)) {
    m_err = Socket::ERROR_INVAL;
    return -1;
  }
  if (m_shutdownSend) {
    return 0;
  }
  InetSocketAddress ad = InetSocketAddress::ConvertFrom (toAddress);
  Ptr<Ipv4> ipv4 = m_node->GetObject<Ipv4> ();
  Ipv4Address dst = ad.GetIpv4 ();
  Ipv4Address src = m_src;
  if (ipv4->GetRoutingProtocol ()) {
    Ipv4Header header;
    header.SetDestination (dst);
    header.SetProtocol (m_protocol);
    // The idea here is to send with PILO control
    header.SetPiloControl (true);
    SocketErrno errno_ = ERROR_NOTERROR; //do not use errno as it is the standard C last error number
    Ptr<Ipv4Route> route;
    Ptr<NetDevice> oif = m_boundnetdevice; //specify non-zero if bound to a source address
    if (!oif && src != Ipv4Address::GetAny ()) {
      int32_t index = ipv4->GetInterfaceForAddress (src);
      NS_ASSERT (index >= 0);
      oif = ipv4->GetNetDevice (index);
      NS_LOG_LOGIC ("Set index " << oif << "from source " << src);
    }

    route = ipv4->GetRoutingProtocol ()->RouteOutput (p, header, oif, errno_);
    if (route != 0) {
      NS_LOG_LOGIC ("Route exists");
      ipv4->SendP (p, route->GetSource (), dst, m_protocol, route, true);
      NotifyDataSent (p->GetSize ());
      NotifySend (GetTxAvailable ());
      return p->GetSize ();
    }
    else {
      NS_LOG_DEBUG ("dropped because no outgoing route.");
      return -1;
    }
  }
  return 0;
}

int
PiloSocket::SendPiloMessage(uint32_t target, PiloMessageType type, Ptr<Packet> packet) {
  NS_LOG_FUNCTION (this << target << type << packet);
  PiloHeader header(m_node->GetId(), target, type, rand_64_num());
  //std::cout << "Constructing header with source " << m_node->GetId() << ", target " << target << ", type " << type << ", ID " << header.GetId() << std::endl;
  packet->AddHeader(header);
  return Socket::Send(packet);
}

bool 
PiloSocket::DeliverPacket (const Ipv4Header& ipHeader) {
  // We could add check for whether the packet is supposed to be received by this node or not, etc.
  NS_LOG_FUNCTION (this << ipHeader);
  // if (!ipHeader.IsPiloControl()) {
  //   return false;
  // }
  

  // if (m_filter.find(piloHeader.GetSourceNode()) != m_filter.end() &&
  //     m_filter[piloHeader.GetSourceNode()].find(header.GetIdentification())  != 
  //     m_filter[piloHeader.GetSourceNode()].end()) {

  // PiloHeader piloHeader;
  // ipHeader.PeekHeader(piloHeader);

  // if (id_seen.find(piloHeader.GetId()) != id_seen.end()) {
  //   return false;
  // }

  // id_seen.insert(piloHeader.GetId());
  // return true;

  return ipHeader.IsPiloControl();
}

}
