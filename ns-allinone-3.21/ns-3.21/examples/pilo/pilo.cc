/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Start with doing some stuff for Pilo.
 */
#include <fstream>
#include <string>
#include <map>
#include <utility>
#include <iostream>
#include <yaml-cpp/yaml.h>
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("PiloBaseSetup");

const std::string LINKS_KEY = "links";
const std::string FAIL_KEY = "fail_links";
const std::string CRIT_KEY = "crit_links";
const std::string RUNFILE_KEY = "runfile";
const std::string TYPE_KEY = "type";
const std::string ARG_KEY = "args";

int32_t ReadNodeInformation (YAML::Node& setupDoc, 
         std::map<std::string, int32_t>& nodeMap,
         std::map<std::string, std::string>& nodeType, 
         std::map<std::string, const YAML::Node&>& nodeArgs) {

  int32_t nodes = 0;
  YAML::Node defaultNode = YAML::Load("[]");
  for (YAML::const_iterator it = setupDoc.begin(); it != setupDoc.end(); ++it) {
      const std::string &key = it->first.as<std::string>();
      if (key == LINKS_KEY ||
          key == FAIL_KEY ||
          key == RUNFILE_KEY ||
          key == CRIT_KEY) {
        // Skip
        continue;
      }
      nodeMap.insert(std::make_pair(key, nodes));
      nodeType.insert(std::make_pair(key, setupDoc[key][TYPE_KEY].as<std::string>()));
      if (setupDoc[key][ARG_KEY]) {
        nodeArgs.insert(std::make_pair(key, setupDoc[key][ARG_KEY]));
      } else {
        nodeArgs.insert(std::make_pair(key, defaultNode));
      }
      nodes++;
  }
  return nodes;
}

int 
main (int argc, char *argv[])
{
//
// Users may find it convenient to turn on explicit debugging
// for selected modules; the below lines suggest how to do this
//
#if 0
  LogComponentEnable ("PiloBaseSetup", LOG_LEVEL_INFO);
#endif
//
// Allow the user to override any of the defaults and the above Bind() at
// run-time, via command-line arguments
//
  Address serverAddress;
  std::string setupYaml;
  CommandLine cmd;
  cmd.AddValue("setup", "YAML file with topology and setup", setupYaml);
  cmd.Parse (argc, argv);

  std::cout << "Using setup file " << setupYaml << std::endl;
  YAML::Node setupDoc = YAML::LoadFile(setupYaml);
  std::cout << "Link size " << setupDoc["links"].size() << std::endl;
  for (int32_t i = 0; i < setupDoc["links"].size(); i++) {
    std::cout << setupDoc["links"][i] << std::endl;
  }
//
// Explicitly create the nodes required by the topology.
//
  std::map<std::string, int32_t> nodeMap;
  std::map<std::string, std::string> nodeType;
  std::map<std::string, const YAML::Node&> nodeArgs;
  NS_LOG_INFO ("Create nodes.");
  int32_t nodes = ReadNodeInformation(setupDoc,
                                      nodeMap,
                                      nodeType,
                                      nodeArgs);
  NodeContainer n;
  n.Create (nodes);

  InternetStackHelper internet;
  internet.Install (n);

  NS_LOG_INFO ("Create channels.");
//
// Explicitly create the channels required by the topology (shown above).
//
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate (5000000)));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  csma.SetDeviceAttribute ("Mtu", UintegerValue (1400));
  NetDeviceContainer d = csma.Install (n);

//
// We've got the "hardware" in place.  Now we need to add IP addresses.
//
  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv6AddressHelper ipv6;
  ipv6.SetBase ("2001:0000:f00d:cafe::", Ipv6Prefix (64));
  Ipv6InterfaceContainer i6 = ipv6.Assign (d);
  serverAddress = Address(i6.GetAddress (1,1));

  NS_LOG_INFO ("Create Applications.");
//
// Create a UdpEchoServer application on node one.
//
  uint16_t port = 9;  // well-known echo port number
  UdpEchoServerHelper server (port);
  ApplicationContainer apps = server.Install (n.Get (1));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

//
// Create a UdpEchoClient application to send UDP datagrams from node zero to
// node one.
//
  uint32_t packetSize = 1024;
  uint32_t maxPacketCount = 1;
  Time interPacketInterval = Seconds (1.);
  UdpEchoClientHelper client (serverAddress, port);
  client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  client.SetAttribute ("Interval", TimeValue (interPacketInterval));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));
  apps = client.Install (n.Get (0));
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (10.0));

#if 0
//
// Users may find it convenient to initialize echo packets with actual data;
// the below lines suggest how to do this
//
  client.SetFill (apps.Get (0), "Hello World");

  client.SetFill (apps.Get (0), 0xa5, 1024);

  uint8_t fill[] = { 0, 1, 2, 3, 4, 5, 6};
  client.SetFill (apps.Get (0), fill, sizeof(fill), 1024);
#endif

  AsciiTraceHelper ascii;
  csma.EnableAsciiAll (ascii.CreateFileStream ("udp-echo.tr"));
  csma.EnablePcapAll ("udp-echo", false);

//
// Now, do the actual simulation.
//
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}
