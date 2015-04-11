/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * PILO code PILO code PILO code
 */
#ifndef PILO_SOCKET_FACTORY_H
#define PILO_SOCKET_FACTORY_H

#include "ns3/socket-factory.h"

namespace ns3 {

class Socket;
class PiloSocketFactory : public SocketFactory
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  virtual Ptr<Socket> CreateSocket (void);

};

} // namespace ns3

#endif
