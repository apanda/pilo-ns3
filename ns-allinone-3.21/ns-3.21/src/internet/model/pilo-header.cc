/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * PILO code PILO code PILO code
 */

#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/header.h"
#include "ns3/simulator.h"
#include "pilo-header.h"

NS_LOG_COMPONENT_DEFINE ("PiloHeader");

namespace ns3 {
NS_OBJECT_ENSURE_REGISTERED (PiloHeader);

PiloHeader::PiloHeader (uint32_t source, 
                        uint32_t target, 
                        PiloMessageType type, 
                        uint64_t id) :
                        m_sourceNode (source),
                        m_targetNode (target),
                        m_type (type),
                        m_id (id) {
}

PiloHeader::PiloHeader() : m_sourceNode (0),
                           m_targetNode (0),
                           m_type (NOP),
                           m_id (0) {
}

uint32_t 
PiloHeader::GetSourceNode () const {
  return m_sourceNode;
}
uint32_t PiloHeader::GetTargetNode () const {
  return m_targetNode;
}
PiloMessageType PiloHeader::GetType () const {
  return m_type;
}
uint64_t
PiloHeader::GetId() const {
  return m_id;
}

TypeId
PiloHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PiloHeader")
    .SetParent<Header> ()
    .AddConstructor<PiloHeader> ()
  ;
  return tid;
}
TypeId
PiloHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}
void
PiloHeader::Print (std::ostream &os) const
{
  NS_LOG_FUNCTION (this << &os);
  os << "(PILOHdr " << m_sourceNode << " " << m_targetNode << " " << m_type << ")";
}
uint32_t
PiloHeader::GetSerializedSize (void) const
{
  NS_LOG_FUNCTION (this);
  return 3*sizeof(uint32_t) + sizeof(uint64_t);
}

void
PiloHeader::Serialize (Buffer::Iterator start) const
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;
  i.WriteHtonU32 (m_sourceNode);
  i.WriteHtonU32 (m_targetNode);
  i.WriteHtonU32 (m_type);
  i.WriteHtonU64 (m_id);
}
uint32_t
PiloHeader::Deserialize (Buffer::Iterator start)
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;
  m_sourceNode = i.ReadNtohU32();
  m_targetNode = i.ReadNtohU32();
  m_type = (PiloMessageType)i.ReadNtohU32();
  m_id = i.ReadNtohU64();
  return GetSerializedSize ();
}

Ptr<Packet>
PiloHeader::CreatePiloAddRoutePacket (const Ipv4Address &addr, uint32_t link) {
  const size_t BUFFER_SIZE = 8;
  Buffer buffer(BUFFER_SIZE);
  Buffer::Iterator i = buffer.Begin();
  i.WriteHtonU32(addr.Get());
  i.WriteHtonU32(link);
  Ptr<Packet> packet = Create<Packet>(buffer);
  return packet;
}

void
PiloHeader::ReadPiloAddRoutePacket(Ipv4Address& addr, uint32_t& link) {
  
}
}
