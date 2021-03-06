/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * PILO code PILO code PILO code
 */
#ifndef PILO_HEADER_H
#define PILO_HEADER_H

#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/header.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/ipv4-address.h"
namespace ns3 {
enum PiloMessageType {
  NOP = 0, // Do nothing, mostly this is to make sure 0 isn't used for anything important
  Echo = 1, // Recepient echos back this packet
  EchoAck = 2, // The echoed back packet
  AddRoute = 3, // Add a single route (i.e., one 
  GossipRequest = 4,
  GossipReply = 5,
  LinkState = 8,
  LinkStateReply = 9,
};

// PILO message header. Essentially just says what the type is etc. 
// TODO: Decide if message content (for example routing table) is linked from here
// or not.
class PiloHeader : public Header {
public:
  // Broadcast address
  static const uint32_t ALL_NODES = 0xffffffff;
  PiloHeader (uint32_t source, uint32_t target, PiloMessageType type, uint64_t id);
  PiloHeader ();
  uint32_t GetSourceNode () const;
  uint32_t GetTargetNode () const;
  PiloMessageType GetType () const;
  uint64_t GetId() const;

  static TypeId GetTypeId (void);

  virtual TypeId GetInstanceTypeId (void) const;
  virtual void Print (std::ostream &os) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  static Ptr<Packet> GetPiloAddRoutePacket (const Ipv4Address &addr, uint32_t link);
  static Ptr<Packet> CreatePiloAddRoutePacket (const Ipv4Address &addr, uint32_t link);
  void ReadPiloAddRoutePacket(Ipv4Address& addr, uint32_t& link);
protected:
  uint32_t m_sourceNode;
  uint32_t m_targetNode;
  PiloMessageType m_type;
  uint64_t m_id;
};
}

#endif
