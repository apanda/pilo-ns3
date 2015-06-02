/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Start with doing some stuff for Pilo.
 */
#include <fstream>
#include <string>
#include <map>
#include <tuple>
#include <list>
#include <vector>
#include <utility>
#include <iostream>
#include <boost/algorithm/string.hpp>
#include "yaml-cpp/yaml.h"
#include "ns3/node.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/error-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("PiloBaseSetup");

const std::string LINKS_KEY = "links";
const std::string FAIL_KEY = "fail_links";
const std::string CRIT_KEY = "crit_links";
const std::string RUNFILE_KEY = "runfile";
const std::string TYPE_KEY = "type";
const std::string ARG_KEY = "args";
const std::string HOST_TYPE = "Host";
const std::string CONTROLLER_TYPE = "Control";
const std::string SWITCH_TYPE = "Switch"; 

static bool EndsWith (std::string const &str, std::string const &end) {
  return (str.length() > end.length()) && 
         (str.compare(str.length() - end.length(), end.length(), end) == 0);
}

int32_t ReadNodeInformation (YAML::Node& setupDoc, 
         std::map<std::string, int32_t>& nodeMap,
         std::map<std::string, std::string>& nodeType, 
         std::map<std::string, const YAML::Node&>& nodeArgs,
         std::list<std::string>& hosts,
         std::list<std::string>& controllers,
         std::list<std::string>& switches) {

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
      std::string type = setupDoc[key][TYPE_KEY].as<std::string>();
      nodeType.insert(std::make_pair(key, type));
      if (setupDoc[key][ARG_KEY]) {
        nodeArgs.insert(std::make_pair(key, setupDoc[key][ARG_KEY]));
      } else {
        nodeArgs.insert(std::make_pair(key, defaultNode));
      }
      if (type.compare(HOST_TYPE) == 0) {
        hosts.push_back(key);
      } else if (EndsWith(type, CONTROLLER_TYPE)) {
        controllers.push_back(key);
      } else if (EndsWith(type, SWITCH_TYPE)) {
        switches.push_back(key);
      } else {
        std::cerr << "Unexpected type " << type << std::endl;
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
  std::list<std::string> hosts;
  std::list<std::string> controllers;
  std::list<std::string> switches;
  std::map<std::tuple<int32_t, int32_t>, Ptr<PointToPointChannel>> channels; 
  NS_LOG_INFO ("Create nodes.");
  int32_t nodes = ReadNodeInformation(setupDoc,
                                      nodeMap,
                                      nodeType,
                                      nodeArgs,
                                      hosts,
                                      controllers,
                                      switches);
  NodeContainer n;
  n.Create (nodes);

  NodeContainer switchContainer;
  NodeContainer controllerContainer;
  NodeContainer hostContainer;

  for (auto host : hosts) {
    hostContainer.Add(n.Get(nodeMap[host]));
  }

  for (auto controller : controllers) {
    controllerContainer.Add(n.Get(nodeMap[controller]));
  }

  for (auto swtch : switches) {
    switchContainer.Add(n.Get(nodeMap[swtch]));
  }


  // The order here (InternetStack installed before p2p links) is meaningful. Reversing this could be bad.
  // Need this to assign IP addresses. Basically this is just installing a new IPv4 stack.
  NS_LOG_INFO ("Install internet stack.");
  InternetStackHelper switchInternet;
  // All the various routing models we use
  Ipv4PiloCtlRoutingHelper ctlRouting;
  Ipv4PiloDPRoutingHelper dpRouting;
  Ipv4ListRoutingHelper switchListRouting;
  // For now, let us not do IPv6. Really no reason for this other than laziness.
  switchInternet.SetIpv6StackInstall(false);
  switchInternet.SetIpv4StackInstall(true);
  // Switches first try using controller routing, then datapath routing.
  switchListRouting.Add (ctlRouting, 100);
  switchListRouting.Add (dpRouting, 0);
  switchInternet.SetRoutingHelper (switchListRouting);
  switchInternet.Install (switchContainer);

  uint32_t switch_id = 0;
  // set the switch IDs
  for (NodeContainer::Iterator it = switchContainer.Begin(); 
       it != switchContainer.End(); it++) {
    // Get node.
    Ptr<Node> node = *it;
    // set switch id
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    Ptr<Ipv4PiloDPRouting> routing = dpRouting.GetPiloDPRouting(ipv4);
    routing->SetSwitchId(switch_id);
    ++switch_id;
  }  

  // Controllers only route PILO packets
  InternetStackHelper controllerInternet;
  controllerInternet.SetIpv6StackInstall(false);
  controllerInternet.SetIpv4StackInstall(true);
  controllerInternet.SetRoutingHelper(ctlRouting);
  controllerInternet.Install (controllerContainer);
  // Hosts just use static routing
  Ipv4StaticRoutingHelper staticRouting;
  InternetStackHelper hostInternet;
  hostInternet.SetIpv6StackInstall(false);
  hostInternet.SetIpv4StackInstall(true);
  hostInternet.SetRoutingHelper(staticRouting);
  hostInternet.Install(hostContainer);
  //
  // Explicitly create the channels required by the topology (specified by the YAML file).
  //
  NS_LOG_INFO ("Create channels.");
  std::list< std::pair<std::string, std::string> > links;
  std::list< std::pair<int32_t, int32_t> > linksNative;
  PointToPointHelper p2p;

  // TODO: Change this/make this more general.
  p2p.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
  p2p.SetChannelAttribute("Delay", TimeValue(Time::FromDouble(0.25, Time::MS))); 


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
    // Hook things ip so link failures are percolated to routing. 
    Ptr<Ipv4> ipv4S = linkNodes.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4D = linkNodes.Get(1)->GetObject<Ipv4>();
    device.Get(0)->AddLinkChangeCallback(MakeBoundCallback(Ipv4::LinkStateCallback, 
                                                          ipv4S,
                                                          ipv4S->GetInterfaceForDevice(device.Get(0))));
    device.Get(1)->AddLinkChangeCallback(MakeBoundCallback(Ipv4::LinkStateCallback, 
                                                          ipv4D,
                                                          ipv4D->GetInterfaceForDevice(device.Get(1))));
    channels[std::make_tuple(nodeMap[parts[0]], nodeMap[parts[1]])] = 
                                            DynamicCast<PointToPointChannel>(device.Get(0)->GetChannel());
    channels[std::make_tuple(nodeMap[parts[1]], nodeMap[parts[0]])] = 
                                            DynamicCast<PointToPointChannel>(device.Get(1)->GetChannel());
  }

  // Assume that hosts are singly homed. Switch off forwarding for hosts and set default path.
  for (NodeContainer::Iterator it = hostContainer.Begin(); 
                it != hostContainer.End(); it++) {
    // Get node.
    Ptr<Node> node = *it;
    // Get IP stack for the node.
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    // Find the static routing thing from the stack.
    Ptr<Ipv4StaticRouting> routing = staticRouting.GetStaticRouting(ipv4);
    // All packets are sent out through node 1.
    routing->AddNetworkRouteTo(Ipv4Address::GetAny(), Ipv4Mask::GetZero(), 1);
    // Do not forward packets received on this interface.
    ipv4->SetForwarding(1, false);
  }


  std::cout << "Found " << links.size() << " links " << std::endl; 
  // std::cout << "server at  " << controllerContainer.Get(0)->GetId() 
  //           << " client at " << controllerContainer.Get(1)->GetId()
  //           << " receiving at " << nodeMap["s1"] << std::endl; 

  std::cout << "Testing controllers at  " << controllerContainer.Get(0)->GetId() << " and "
            << controllerContainer.Get(1)->GetId() << std::endl; 

  /************ BEGIN PILO CONTROLLER TEST ************/

  uint32_t MaxPacketSize = 1024;
  Time interPacketInterval = Seconds (0.05);
  uint32_t maxPacketCount = 1;

  Ptr<Node> cNode1 = controllerContainer.Get(0);
  Ptr<Node> cNode2 = controllerContainer.Get(1);
  uint16_t port = 6500;
  Ipv4Address serverAddress1 = cNode1->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
  Ipv4Address serverAddress2 = cNode2->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();

  // Create a test controller
  PiloControllerHelper controller1(serverAddress1, port);
  controller1.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  controller1.SetAttribute ("Interval", TimeValue (interPacketInterval));
  controller1.SetAttribute ("PacketSize", UintegerValue (MaxPacketSize));
  //controller1.SetAttribute ("NodeSend", UintegerValue (nodeMap["h0"]));
  //controller1.SetAttribute ("NodeSend", UintegerValue (controllerContainer.Get(1)->GetId()));

  // Create a test controller
  PiloControllerHelper controller2(serverAddress2, port);
  controller2.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  controller2.SetAttribute ("Interval", TimeValue (interPacketInterval));
  controller2.SetAttribute ("PacketSize", UintegerValue (MaxPacketSize));
  controller2.SetAttribute ("NodeSend", UintegerValue (nodeMap["s1"]));
  //controller2.SetAttribute ("NodeSend", UintegerValue (controllerContainer.Get(0)->GetId()));
  
  ApplicationContainer apps;
  apps = controller1.Install (cNode1);
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (700.0));  

  apps = controller2.Install (cNode2);
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (700.0));

  // partition the network
  Simulator::Schedule(Seconds(6), &PointToPointChannel::SetLinkDown, channels[std::make_tuple(nodeMap["s4"], nodeMap["s5"])]);

  // fail and recover links
  Simulator::Schedule(Seconds(7), &PointToPointChannel::SetLinkDown, channels[std::make_tuple(nodeMap["s7"], nodeMap["s8"])]);
  Simulator::Schedule(Seconds(7), &PointToPointChannel::SetLinkDown, channels[std::make_tuple(nodeMap["s1"], nodeMap["s3"])]);

  Simulator::Schedule(Seconds(8), &PointToPointChannel::SetLinkUp, channels[std::make_tuple(nodeMap["s7"], nodeMap["s8"])]);
  Simulator::Schedule(Seconds(8), &PointToPointChannel::SetLinkUp, channels[std::make_tuple(nodeMap["s1"], nodeMap["s3"])]);

  Simulator::Schedule(Seconds(9), &PointToPointChannel::SetLinkDown, channels[std::make_tuple(nodeMap["s7"], nodeMap["s8"])]);
  Simulator::Schedule(Seconds(9), &PointToPointChannel::SetLinkDown, channels[std::make_tuple(nodeMap["s1"], nodeMap["s3"])]);

  Simulator::Schedule(Seconds(10), &PointToPointChannel::SetLinkUp, channels[std::make_tuple(nodeMap["s7"], nodeMap["s8"])]);
  Simulator::Schedule(Seconds(10), &PointToPointChannel::SetLinkUp, channels[std::make_tuple(nodeMap["s1"], nodeMap["s3"])]);

  Simulator::Schedule(Seconds(11), &PointToPointChannel::SetLinkUp, channels[std::make_tuple(nodeMap["s4"], nodeMap["s5"])]);

  /************ END PILO CONTROLLER TEST ************/

  // // Create a UDP server to start of
  // Ptr<Node> serverNode = controllerContainer.Get(0);
  // uint16_t port = 6500;
  // PiloCtlServerHelper server (port);
  // ApplicationContainer apps = server.Install (serverNode);
  // apps.Start (Seconds (1.0));
  // apps.Stop (Seconds (10.0));
  // Ipv4Address serverAddress = serverNode->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
  // // Create a UDP client
  // Ptr<Node> clientNode = controllerContainer.Get(1);
  // uint32_t MaxPacketSize = 1024;
  // Time interPacketInterval = Seconds (0.05);
  // uint32_t maxPacketCount = 1;
  // PiloCtlClientHelper client (serverAddress, port);
  // client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  // client.SetAttribute ("Interval", TimeValue (interPacketInterval));
  // client.SetAttribute ("PacketSize", UintegerValue (MaxPacketSize));
  // //client.SetAttribute ("NodeSend", UintegerValue (nodeMap["h0"]));
  // client.SetAttribute ("NodeSend", UintegerValue (nodeMap["s1"]));
  // apps = client.Install (clientNode);
  // apps.Start (Seconds (2.0));
  // apps.Stop (Seconds (700.0));

  // Demonstrate how to set a link down 10 seconds in simulation time
  //Simulator::Schedule(Seconds(10), &PointToPointChannel::SetLinkDown, channels[std::make_tuple(nodeMap["s6"], nodeMap["s49"])]);

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
