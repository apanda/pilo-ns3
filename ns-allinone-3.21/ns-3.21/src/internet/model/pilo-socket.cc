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

int 
PiloSocket::SendTo (Ptr<Packet> p, uint32_t flags, 
                           const Address &toAddress)
{
  NS_LOG_FUNCTION (this << p << flags << toAddress);
  if (!InetSocketAddress::IsMatchingType (toAddress))
    {
      m_err = Socket::ERROR_INVAL;
      return -1;
    }
  if (m_shutdownSend)
    {
      return 0;
    }
  InetSocketAddress ad = InetSocketAddress::ConvertFrom (toAddress);
  Ptr<Ipv4> ipv4 = m_node->GetObject<Ipv4> ();
  Ipv4Address dst = ad.GetIpv4 ();
  Ipv4Address src = m_src;
  if (ipv4->GetRoutingProtocol ())
    {
      Ipv4Header header;
      if (!m_iphdrincl)
        {
          header.SetDestination (dst);
          header.SetProtocol (m_protocol);
        }
      else
        {
          p->RemoveHeader (header);
          dst = header.GetDestination ();
          src = header.GetSource ();
        }
      // The idea here is to send with PILO control
      header.SetPiloControl (true);
      SocketErrno errno_ = ERROR_NOTERROR; //do not use errno as it is the standard C last error number
      Ptr<Ipv4Route> route;
      Ptr<NetDevice> oif = m_boundnetdevice; //specify non-zero if bound to a source address
      if (!oif && src != Ipv4Address::GetAny ())
        {
          int32_t index = ipv4->GetInterfaceForAddress (src);
          NS_ASSERT (index >= 0);
          oif = ipv4->GetNetDevice (index);
          NS_LOG_LOGIC ("Set index " << oif << "from source " << src);
        }

      // TBD-- we could cache the route and just check its validity
      route = ipv4->GetRoutingProtocol ()->RouteOutput (p, header, oif, errno_);
      if (route != 0)
        {
          NS_LOG_LOGIC ("Route exists");
          if (!m_iphdrincl)
            {
              ipv4->SendP (p, route->GetSource (), dst, m_protocol, route, true);
            }
          else
            {
              ipv4->SendWithHeader (p, header, route);
            }
          NotifyDataSent (p->GetSize ());
          NotifySend (GetTxAvailable ());
          return p->GetSize ();
        }
      else
        {
          NS_LOG_DEBUG ("dropped because no outgoing route.");
          return -1;
        }
    }
  return 0;
}

}
