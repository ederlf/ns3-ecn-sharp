/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/* Author: Antonio Marsico <antonio.marsico@bt.com> */

#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
#include <unistd.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unordered_map>

#include "ns3/flow-monitor-module.h"
#include "ns3/bridge-helper.h"
#include "ns3/bridge-net-device.h"
#include "ns3/core-module.h"
#include "ns3/queue.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-nix-vector-helper.h"
#include "ns3/rpc-generator-helper.h"
#include "ns3/rpc-application.h"
#include "ns3/hula-routing-helper.h"
#include "ns3/hula-generator-helper.h"
#include "ns3/animation-interface.h"


/*
- The code is constructed in the following order:
1. Creation of Node Containers
2. Initialize settings for UdpEcho Client/Server Application
3. Connect hosts to edge switches
4. Connect edge switches to aggregate switches
5. Connect aggregate switches to core switches
6. Start Simulation

- Addressing scheme:
1. Address of host: 10.pod.edge.0 /24
2. Address of edge and aggregation switch: 10.pod.(agg + pod/2).0 /24
3. Address of core switch: 10.(group + pod).core.0 /24
(Note: there are k/2 group of core switch)

- Application Setting:
- PacketSize: (Send packet data size (byte))
- Interval: (Send packet interval time (s))
- MaxPackets: (Send packet max number)

- Channel Setting:
- DataRate: (Gbps)
- Delay: (ms)

- Simulation Settings:
- Number of pods (k): 4-24 (run the simulation with varying values of k)
- Number of hosts: 16-3456
- UdpEchoClient/Server Communication Pairs:
[k] client connect with [hostNum-k-1] server (k from 0 to halfHostNum-1)
- Simulation running time: 100 seconds
- Traffic flow pattern:
- Routing protocol: Nix-Vector
*/


using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HULAFatTreeExample");

// Function to create address string from numbers
//
char * toString(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {

	uint32_t first = a;
	uint32_t second = b;
	uint32_t third = c;
	uint32_t fourth = d;

	char *address = new char[30];
	char firstOctet[30], secondOctet[30], thirdOctet[30], fourthOctet[30];

	bzero(address, 30);

	snprintf(firstOctet, 10, "%d", first);
	strcat(firstOctet, ".");
	snprintf(secondOctet, 10, "%d", second);
	strcat(secondOctet, ".");
	snprintf(thirdOctet, 10, "%d", third);
	strcat(thirdOctet, ".");
	snprintf(fourthOctet, 10, "%d", fourth);

	strcat(thirdOctet, fourthOctet);
	strcat(secondOctet, thirdOctet);
	strcat(firstOctet, secondOctet);
	strcat(address, firstOctet);

	return address;
}

//typedef std::unordered_map<uint64_t, uint32_t>::iterator TupleIter_t;

//Output switch received packet info
//
void ShowSwitchInfos(Ptr<Node> s, std::vector<double> &fcts, std::vector<uint32_t> &connections)
{
	for (uint32_t i = 0; i < s->GetNApplications(); i++) {
		Ptr<RPCApplication> rpcgen = DynamicCast<RPCApplication> (s->GetApplication (i));
		if (rpcgen != nullptr) {
			NS_LOG_INFO("FCT Sum App " << i << " Node " << s->GetId() << ": " << rpcgen->GetTotalFCT() );
			fcts[0] += rpcgen->GetTotalFCT();
            fcts[1] += rpcgen->GetTotalLongFCT();
            fcts[2] += rpcgen->GetTotalShortFCT();

			connections[0] += rpcgen->GetTotalConnections();
			connections[1] += rpcgen->GetTotalLongConnections();
			connections[2] += rpcgen->GetTotalShortConnections();
		}
	}
}

int
main(int argc, char *argv[])
{
	LogComponentEnable("HULAFatTreeExample", LOG_LEVEL_DEBUG);


	//LogComponentEnable("PointToPointNetDevice", LOG_LEVEL_LOGIC);
	//LogComponentEnable("CsmaNetDevice",LOG_LEVEL_LOGIC);

	uint32_t pod = 2;
	// Initialize parameters for UdpEcho Client/Server application
	//
	uint16_t port = 9;
	uint32_t packetSize = 1460;		// send packet data size (byte)
	uint32_t maxRPCConnections = 20;
	uint32_t algorithm = 1;
	double flow_interval = 24.455770357; // Flow/sec.
    double hulaStartTime = 0.1; //seconds
    uint32_t hula_interval = 100; //microseconds
    bool verbose = false;
    bool queue = false;
    uint32_t traffic_type = 1;
    uint32_t totalRPCApplications = 3;
    uint32_t queueSize = 50;


	// Initialize parameters for Csma and PointToPoint protocol
	//
	char dataRate[] = "40Gbps";	// 40Gbps
    char leafDataRate[] = "10Gbps";
	std::string stringDelay = "0.002ms";
    std::string stringLeafDelay = "0.025ms";

	// Initalize parameters for RPC Client/Server Appilication
	//
	double clientStartTime = 0.15; // RPC Start Time (s)
	double clientStopTime = 4;
	double serverStartTime = 0.1; // RPC Start Time (s)
    double serverStopTime = 4.5;

    
    Config::SetDefault ("ns3::TcpSocketBase::MinRto", TimeValue(MilliSeconds(200)));
    Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue(packetSize));



	CommandLine cmd;
	cmd.AddValue("podnum", "Numbers of pod [default 2]", pod);
	cmd.AddValue("inter", "UdpEchoClient send packet interval time/s [default 100ms]", flow_interval);
    cmd.AddValue("packetSize", "RPC Generator Packet Size [default 1460 bytes]", packetSize);
    cmd.AddValue("maxConnections", "RPC Generator Max Connections [default 20]", maxRPCConnections);
    cmd.AddValue("traffic", "1 for Web Search, 2 for Data Mining [default 1]", traffic_type);
    cmd.AddValue("algo", "Algorithm to use 1:ECMP, 2:HULA, 3:Heavy Hitters [default 1]", algorithm);
    cmd.AddValue("hulaInterval", "HULA probe interval [default 100 microsec]", hula_interval);
	cmd.AddValue("cs", " Client application start time/s [default 1s]", clientStartTime);
	cmd.AddValue("ce", " Client application stop time/s [default 4s]", clientStopTime);
	cmd.AddValue("ss", " Server application start time/s [default 0s]", serverStartTime);
	cmd.AddValue("se", " Server application stop time/s [default 5s]", serverStopTime);
    cmd.AddValue ("verbose", "Turn on all device log components", verbose);
    cmd.AddValue ("queue", "Use queue depth instead of link congestion in HULA", queue);
    cmd.AddValue ("queueSize", "Size of the queue [50p default]", queueSize);
	cmd.Parse(argc, argv);

	if (verbose) {
        LogComponentEnable("HULAFatTreeExample", LOG_LEVEL_INFO);
        LogComponentEnable("RPCApplication", LOG_LEVEL_DEBUG);
        LogComponentEnable("HULARouting", LOG_LEVEL_DEBUG);
	}

	NS_LOG_INFO( "------------Network Parameters Setting-----------------" );
	NS_LOG_INFO( "NS3 Simulate Mode:" );
    NS_LOG_INFO( "Send Flow Interval: " << flow_interval << " fps" );
    NS_LOG_INFO( "Send Packet Data Size: " << packetSize << " byte" );
    NS_LOG_INFO( "Queue depth: " << queue << " size " << queueSize );

    Config::SetDefault ("ns3::Queue::MaxPackets", UintegerValue(queueSize));


	//=========== Define parameters based on value of k ===========//
	//
	uint32_t k = pod;			// number of ports per switch
	uint32_t num_pod = k;		// number of pod
	uint32_t num_host = 8;		// number of hosts under a switch
	uint32_t num_edge = 2;		// number of edge switch in a pod
	uint32_t num_bridge = 1;	// number of bridge in a pod
	uint32_t num_agg = 2;		// number of aggregation switch in a pod
	uint32_t num_group = 2;		// number of group of core switches
	uint32_t num_core = num_agg;		// number of core switch in a group
	uint32_t total_host = num_host * num_edge * num_bridge * num_pod;	// number of hosts in the entire network

								// Initialize other variables
								//
	uint32_t i = 0;
	uint32_t j = 0;
	uint32_t h = 0;


    NS_LOG_INFO( "Total number of hosts =  " << total_host );
    NS_LOG_INFO( "Number of hosts under each switch =  " << num_host );
    NS_LOG_INFO( "Number of edge switch under each pod =  " << num_edge );
    NS_LOG_INFO( "------------- " );

	if (algorithm == 1)
	{
		Config::SetDefault("ns3::Ipv4GlobalRouting::RandomEcmpRouting", BooleanValue(true));
	}

	if (algorithm == 3)
	{
		Config::SetDefault("ns3::Ipv4GlobalRouting::HeavyHitterRouting", BooleanValue(true));
	}

	// Initialize Internet Stack and Routing Protocols
	//	
	InternetStackHelper internet;
    InternetStackHelper edgeInternet;
    InternetStackHelper hostRouting;
	//Ipv4NixVectorHelper nixRouting;
    Ipv4GlobalRoutingHelper nixRouting;
	Ipv4StaticRoutingHelper staticRouting;
	Ipv4ListRoutingHelper list;
	Ipv4ListRoutingHelper edgeRoutingList;
	list.Add(staticRouting, 0);
    edgeRoutingList.Add(staticRouting, 0);
	list.Add(nixRouting, 10);
    edgeRoutingList.Add(nixRouting, 10);


	if (algorithm == 2) {
	    HULARoutingHelper hulaRoutingHelper;
        HULARoutingHelper hulaEdgeRoutingHelper;
        hulaEdgeRoutingHelper.Set("IsEdge", BooleanValue(true));


        if (queue) {
            hulaRoutingHelper.Set("queue", BooleanValue(true));
            hulaEdgeRoutingHelper.Set("queue", BooleanValue(true));
        }

        edgeRoutingList.Add(hulaEdgeRoutingHelper, 20);
        list.Add(hulaRoutingHelper, 20);
	}
	

	internet.SetRoutingHelper(list);
    edgeInternet.SetRoutingHelper(edgeRoutingList);

	//=========== Creation of Node Containers ===========//
	//
	NodeContainer core[num_group];				// NodeContainer for core switches
	for (i = 0; i<num_group; i++) {
		core[i].Create(num_core);
		//Names::Add("Core1", core[i].Get(0));
		internet.Install(core[i]);
	}

	NodeContainer agg[num_pod];				// NodeContainer for aggregation switches
	for (i = 0; i<num_pod; i++) {
		agg[i].Create(num_agg);
		internet.Install(agg[i]);
	}
	NodeContainer edge[num_pod];				// NodeContainer for edge switches
	for (i = 0; i<num_pod; i++) {
		edge[i].Create(num_edge);
        edgeInternet.Install(edge[i]);
	}

	NodeContainer host[num_pod][num_edge];		// NodeContainer for hosts
	for (i = 0; i<num_pod; i++) {
		for (j = 0; j<num_edge; j++) {
			host[i][j].Create(num_host);
			hostRouting.Install(host[i][j]);
		}
	}

    NS_LOG_INFO( "Finished creating Node Containers" );

	//=========== Initialize settings for UdpEcho Client/Server Application ===========//
	//

	// Generate traffics for the simulation
	//	

	//Config::SetDefault("ns3::Ipv4RawSocketImpl::Protocol", StringValue("2"));

	ApplicationContainer app[total_host];

	char *addr;
	uint32_t half_host_num = total_host / 2;

	int64_t current_stream = 0;

	for (uint32_t client_i = 0; client_i < half_host_num; client_i++)
	{
		uint32_t server_i = total_host - client_i - 1;

		uint32_t s_p, s_q, s_t;
		s_p = server_i / (num_edge*num_host);
		s_q = (server_i - s_p*num_edge*num_host) / num_host;
		s_t = server_i%num_host;

        //NS_LOG_INFO( "Parametri server " << s_p << " " << s_q << " " << s_t ); 30, 7

		uint32_t p, q, t;
		p = client_i / (num_edge*num_host);
		q = (client_i - p*num_edge*num_host) / num_host;
		t = client_i%num_host;

        //NS_LOG_INFO( "Parametri client " << p << " " << q << " " << t );
		//specify dst ip (server ip)

		addr = toString(10, s_p, s_q, (s_t * 4) + 2);
		Ipv4Address dstIp = Ipv4Address(addr);


		//install server app
		PacketSinkHelper sink ("ns3::TcpSocketFactory",
							   InetSocketAddress (Ipv4Address::GetAny (), port));

		ApplicationContainer serverApp = sink.Install(host[s_p][s_q].Get(s_t));
		serverApp.Start(Seconds(serverStartTime));
		serverApp.Stop(Seconds(serverStopTime));

		//install client app
		RPCGeneratorHelper rpcGeneratorHelper ("ns3::TcpSocketFactory",
				InetSocketAddress(dstIp, port));

		rpcGeneratorHelper.SetAttribute("FlowRate", DoubleValue(flow_interval));
        rpcGeneratorHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
        rpcGeneratorHelper.SetAttribute("MaxConnections", UintegerValue(maxRPCConnections));

        if (traffic_type == 2) {
            rpcGeneratorHelper.SetAttribute("FilePath", StringValue("./src/hula/examples/cdf_vl2_datamining.txt"));
        }


        //if (p==0 && q == 1 && t==4) {
        //if (client_i == 12) {
        app[client_i] = rpcGeneratorHelper.Install(host[p][q].Get(t));
        for (uint32_t appIndex = 1; appIndex < totalRPCApplications; appIndex++) {
            app[client_i].Add(rpcGeneratorHelper.Install(host[p][q].Get(t)));
        }

		//Assign a different stream of random values to each RPC application
		current_stream += rpcGeneratorHelper.AssignStreams(host[p][q], current_stream);

        app[client_i].Start(Seconds(clientStartTime));
        app[client_i].Stop(Seconds(clientStopTime));
        //}


        NS_LOG_INFO("Client ID " << p << "," << q << "," << t << " Server IP " << dstIp);
        //}

        if (algorithm == 2 && t == 0) {

            HulaGeneratorHelper hulaProbeGeneratorHelperClient;
            hulaProbeGeneratorHelperClient.SetGeneratorAttribute("Interval", TimeValue(MicroSeconds(hula_interval)));
            hulaProbeGeneratorHelperClient.SetGeneratorAttribute("Address", Ipv4AddressValue(toString(10, p, q, 0)));
            hulaProbeGeneratorHelperClient.SetGeneratorAttribute("Mask", Ipv4MaskValue("/24"));

            ApplicationContainer hulaApplicationContainer = hulaProbeGeneratorHelperClient.Install(host[p][q].Get(t));

            hulaApplicationContainer.Start(Seconds(hulaStartTime));
            hulaApplicationContainer.Stop(Seconds(clientStopTime));

        }

        if (algorithm == 2 && s_t == 0) {

            HulaGeneratorHelper hulaProbeGeneratorHelperServer;
            hulaProbeGeneratorHelperServer.SetGeneratorAttribute("Interval", TimeValue(MicroSeconds(hula_interval)));
            hulaProbeGeneratorHelperServer.SetGeneratorAttribute("Address", Ipv4AddressValue(toString(10, s_p, s_q, 0)));
            hulaProbeGeneratorHelperServer.SetGeneratorAttribute("Mask", Ipv4MaskValue("/24"));

            ApplicationContainer hulaApplicationContainer = hulaProbeGeneratorHelperServer.Install(host[s_p][s_q].Get(s_t));

            hulaApplicationContainer.Start(Seconds(hulaStartTime));
            hulaApplicationContainer.Stop(Seconds(clientStopTime));

        }
	}

	

    NS_LOG_INFO( "Finished creating RPC clients/servers traffic" );


	// Inintialize Address Helper
	//	
	Ipv4AddressHelper address;

	// Initialize PointtoPoint helper
	//	
	PointToPointHelper p2p;
	p2p.SetDeviceAttribute("DataRate", StringValue(dataRate));
	p2p.SetChannelAttribute("Delay", StringValue(stringDelay));

    PointToPointHelper leafP2P;
    leafP2P.SetDeviceAttribute("DataRate", StringValue(leafDataRate));
    leafP2P.SetChannelAttribute("Delay", StringValue(stringLeafDelay));


	//=========== Connect edge switches to hosts ===========//
	//	
	NetDeviceContainer hostSw[num_pod][num_edge][num_host];
	Ipv4InterfaceContainer ipContainer[num_pod][num_edge][num_host];

	for (i = 0; i<num_pod; i++) {
		for (j = 0; j<num_edge; j++) {
            uint32_t fourth_octet = 0;
            for (h = 0; h < num_host; h++) {

                NS_LOG_INFO( "Connecting [pod,edge,host] [" << i << "," << j << "," << h << "]");

                hostSw[i][j][h] = leafP2P.Install(edge[i].Get(j), host[i][j].Get(h));

                //pAnim->SetConstantPosition(edge[i].Get(j), i, j);

                char *subnet;
                subnet = toString(10, i, j, fourth_octet);
                char *defaultGateway = toString(10, i, j, fourth_octet+1);
                address.SetBase(subnet, "/30");

                Ptr<Ipv4StaticRouting> staticRoutingInstance;
                staticRoutingInstance = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (host[i][j].Get(h)->GetObject<Ipv4> ()->GetRoutingProtocol ());
                staticRoutingInstance->SetDefaultRoute (defaultGateway, 1 );

                ipContainer[i][j][h] = address.Assign(hostSw[i][j][h]);
                fourth_octet += 4;
            }
		}
	}
    NS_LOG_INFO( "Finished connecting edge switches and hosts  " );

	//=========== Connect aggregate switches to edge switches ===========//
	//
	NetDeviceContainer ae[num_pod][num_agg][num_edge];
	Ipv4InterfaceContainer ipAeContainer[num_pod][num_agg][num_edge];
	for (i = 0; i<num_pod; i++) {
		for (j = 0; j<num_agg; j++) {
            uint32_t fourth_octet = 0;
			for (h = 0; h<num_edge; h++) {

                NS_LOG_INFO( "Connecting [pod,aggregate,edge] [" << i << "," << j << "," << h << "]");

				ae[i][j][h] = p2p.Install(agg[i].Get(j), edge[i].Get(h));

				uint32_t second_octet = i;
				uint32_t third_octet = j + (k + 4);
				//uint32_t fourth_octet;
				//if (h == 0) fourth_octet = 1;
				//else fourth_octet = h * 2 + 1;
				//Assign subnet
				char *subnet;
				subnet = toString(11, second_octet, third_octet, fourth_octet);
				NS_LOG_INFO("Subnet " << subnet);
				//Assign base
				//char *base;
				//base = toString(0, 0, 0, fourth_octet);
				address.SetBase(subnet, "/30");
				ipAeContainer[i][j][h] = address.Assign(ae[i][j][h]);
                fourth_octet += 4;
			}
		}
	}
    NS_LOG_INFO( "Finished connecting aggregation switches and edge switches  " );

	//=========== Connect core switches to aggregate switches ===========//
	//
	NetDeviceContainer ca[num_group][num_core][num_pod];
	Ipv4InterfaceContainer ipCaContainer[num_group][num_core][num_pod];
	uint32_t fourth_octet = 1;

	for (i = 0; i<num_group; i++) {
		for (j = 0; j < num_core; j++) {
			fourth_octet = 0;
			for (h = 0; h < num_pod; h++) {
				ca[i][j][h] = p2p.Install(core[i].Get(j), agg[h].Get(i));

                NS_LOG_INFO( "Connecting [group-aggregate,core,pod] [" << i << "," << j << "," << h << "]");
				uint32_t second_octet = k + i;
				uint32_t third_octet = j;
				//Assign subnet
				char *subnet;
				subnet = toString(12, second_octet, third_octet, fourth_octet);
				//Assign base
				//char *base;
				//base = toString(0, 0, 0, fourth_octet);
				address.SetBase(subnet, "/30");
				ipCaContainer[i][j][h] = address.Assign(ca[i][j][h]);
				fourth_octet += 4;
			}
		}
	}
    NS_LOG_INFO( "Finished connecting core switches and aggregation switches  " );
    NS_LOG_INFO( "------------- " );


	//=========== Start the simulation ===========//
	//

    NS_LOG_INFO( "Start Simulation.. " );

    //app[0].Start(Seconds(clientStartTime));
    //app[0].Stop(Seconds(clientStopTime));

	/*for (i = 0; i<half_host_num; i++) {
		app[i].Start(Seconds(clientStartTime));
		app[i].Stop(Seconds(clientStopTime));
	}*/
	Ipv4GlobalRoutingHelper::PopulateRoutingTables();

	// Calculate Throughput using Flowmonitor
	//
	//  FlowMonitorHelper flowmon;
	//  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    //AsciiTraceHelper ascii;
    //csma.EnableAsciiAll (ascii.CreateFileStream ("hula-trace-example.tr"));

    //Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("hula-routing-tables.routes", std::ios::out);
    //HULARoutingHelper::PrintRoutingTableAllAt (Seconds (1), routingStream);

	// Run simulation.
	//
    NS_LOG_INFO("Run Simulation.");

	Simulator::Stop(Seconds(serverStopTime + 1));
	//Packet::EnablePrinting();

	Simulator::Run();


    NS_LOG_INFO( "Simulation finished " );

	//=========== Show Host FCT ===========//
	//
	NS_LOG_INFO( "============= Show Host Received Packet Num ==============" );

	double totalFCT = 0.0;
    double totalFCT_long = 0.0;
    double totalFCT_short = 0.0;

	uint32_t total_connections = 0;
    uint32_t total_connections_long = 0;
    uint32_t total_connections_short = 0;
	i = 0;
    //j=1;
	//k=4;

    for (j = 0; j < num_edge; j++)
    {
        for (k = 0; k < num_host; k++)
        {
            NS_LOG_INFO( "Host [pod,edge,host] [" << i << "," << j << "," << k << "]:");

			std::vector<double> fcts = {0.0, 0.0, 0.0};
			std::vector<uint32_t> connections = {0, 0, 0};

            ShowSwitchInfos(host[i][j].Get(k), fcts, connections);
            totalFCT += fcts[0];
            totalFCT_long += fcts[1];
            totalFCT_short += fcts[2];

			total_connections += connections[0];
            total_connections_long += connections[1];
            total_connections_short += connections[2];
        }
    }

    NS_LOG_DEBUG(totalFCT/total_connections << " " << totalFCT_long/total_connections_long << " " << totalFCT_short/total_connections_short);

	NS_LOG_INFO( "==========================================================" );

	Simulator::Destroy();
	NS_LOG_INFO("Done.");
	return 0;
}




