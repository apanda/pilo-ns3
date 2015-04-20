/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * PILO code PILO code PILO code
 */
#ifndef PILO_SOCKET_H
#define PILO_SOCKET_H
#include "ipv4-raw-socket-impl.h"
#include "pilo-header.h"
namespace ns3 {
class PiloSocket : public Ipv4RawSocketImpl {
public:
  /**
   * \brief Get the type ID of this class.
   * \return type ID
   */
  static TypeId GetTypeId (void);
  PiloSocket ();
  int SendPiloMessage (uint32_t target,
            PiloMessageType type,
            Ptr<Packet> packet);
  virtual int SendTo (Ptr<Packet> p, uint32_t flags, 
                      const Address &toAddress);
protected:
  static const uint16_t PROTOCOL = 0xfe;
};
}
#endif
