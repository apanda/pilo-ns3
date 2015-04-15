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
                        PiloMessageType type) :
                        m_sourceNode (source),
                        m_targetNode (target),
                        m_type (type) {
}

PiloHeader::PiloHeader() : m_sourceNode (0),
                        m_targetNode (0),
                        m_type (NOP) {
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
  return 3*sizeof(uint32_t);
}

void
PiloHeader::Serialize (Buffer::Iterator start) const
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;
  i.WriteHtonU32 (m_sourceNode);
  i.WriteHtonU32 (m_targetNode);
  i.WriteHtonU32 (m_type);
}
uint32_t
PiloHeader::Deserialize (Buffer::Iterator start)
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;
  m_sourceNode = i.ReadNtohU32();
  m_targetNode = i.ReadNtohU32();
  m_type = (PiloMessageType)i.ReadNtohU32();
  return GetSerializedSize ();
}
}
