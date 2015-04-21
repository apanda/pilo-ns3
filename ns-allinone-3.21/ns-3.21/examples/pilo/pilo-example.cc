/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Start with doing some stuff for Pilo.
 */
#include <fstream>
#include <string>
#include <map>
#include <list>
#include <vector>
#include <utility>
#include <iostream>
#include <boost/algorithm/string.hpp>
#include "yaml-cpp/yaml.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"

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
  YAML::Node defaultNode = YAML::Load(std::string("[]"));
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
  std::string setupYaml;
  CommandLine cmd;
  cmd.AddValue("setup", "YAML file with topology and setup", setupYaml);
  cmd.Parse (argc, argv);

  std::cout << "Using setup file " << setupYaml << std::endl;
  YAML::Node setupDoc = YAML::LoadFile(setupYaml);
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


//
// Explicitly create the channels required by the topology (specified by the YAML file).
//
  NS_LOG_INFO ("Create channels.");
  std::list< std::pair<std::string, std::string> > links;
  std::list< std::pair<int32_t, int32_t> > linksNative;
  PointToPointHelper p2p;

  // The order here (InternetStack installed before p2p links) is meaningful. Reversing this could be bad.
  // Need this to assign IP addresses. Basically this is just installing a new IPv4 stack.
  NS_LOG_INFO ("Install internet stack.");
  InternetStackHelper internet;
  // For now, let us not do IPv6. Really no reason for this other than laziness.
  internet.SetIpv6StackInstall(false);
  internet.SetIpv4StackInstall(true);

  // TODO: Change this/make this more general.
  p2p.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
  p2p.SetChannelAttribute("Delay", TimeValue(Time::FromDouble(0.25, Time::MS))); 


  // TODO: Switch to PILO version, but roughly this for now.
  // We want to just allow static routes. Nesting it within list routing is helpful in terms of adding
  // other strategies later. For example, we would really want a PILO control router at high priority and
  // a data plane router at higher priority.
  Ipv4PiloCtlRoutingHelper ctlRouting;
  Ipv4PiloDPRoutingHelper dpRouting;
  Ipv4ListRoutingHelper listRouting;
  // We want ctlRouting to have the highest priority
  listRouting.Add (ctlRouting, 100);
  listRouting.Add (dpRouting, 0);
  internet.SetRoutingHelper (listRouting);
  internet.Install (n);

  // Use this to assign IPv4 addresses.
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.0.0.0", "255.255.255.0");
  // A collection of all the newly added IPv4 interfaces 
  Ipv4InterfaceContainer addrs;

  NetDeviceContainer netDevices;
  for (uint32_t i = 0; i < setupDoc["links"].size(); i++) {
    std::vector<std::string> parts;
    boost::split(parts, setupDoc["links"][i].as<std::string>(), boost::is_any_of("-"));
    links.push_back(std::make_pair(parts[0], parts[1]));
    linksNative.push_back(std::make_pair(nodeMap[parts[0]], nodeMap[parts[1]]));
    NodeContainer linkNodes(n.Get(nodeMap[parts[0]]));
    linkNodes.Add(n.Get(nodeMap[parts[1]]));
    NetDeviceContainer device = p2p.Install(linkNodes);
    netDevices.Add(device);
    addrs.Add(ipv4.Assign(device));
    ipv4.NewNetwork();
  }

  std::cout << "Found " << links.size() << " links " << std::endl; 
  std::cout << "server at  " << nodeMap["h0"] 
            << " client at " << nodeMap["h1"] 
            << " receiving at " << nodeMap["s1"] << std::endl; 

  // Create a UDP server to start of
  Ptr<Node> serverNode = n.Get(nodeMap["h0"]);
  uint16_t port = 6500;
  PiloCtlServerHelper server (port);
  ApplicationContainer apps = server.Install (serverNode);
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));
  Ipv4Address serverAddress = serverNode->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();

  // Create a UDP client
  Ptr<Node> clientNode = n.Get(nodeMap["h1"]);
  uint32_t MaxPacketSize = 1024;
  Time interPacketInterval = Seconds (0.05);
  uint32_t maxPacketCount = 1;
  PiloCtlClientHelper client (serverAddress, port);
  client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  client.SetAttribute ("Interval", TimeValue (interPacketInterval));
  client.SetAttribute ("PacketSize", UintegerValue (MaxPacketSize));
  //client.SetAttribute ("NodeSend", UintegerValue (nodeMap["h0"]));
  client.SetAttribute ("NodeSend", UintegerValue (nodeMap["s1"]));
  apps = client.Install (clientNode);
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (700.0));


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

//
// Now, do the actual simulation.
//
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}
