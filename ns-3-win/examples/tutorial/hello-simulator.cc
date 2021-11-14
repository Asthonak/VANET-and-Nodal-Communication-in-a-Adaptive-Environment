/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// This i s the t o p ol o g y o f our network :
//
// node 0 node 1
// +−−−−−−−−−−−−−−−−+ +−−−−−−−−−−−−−−−−+
// | ns−3 TCP | | ns−3 TCP |
// +−−−−−−−−−−−−−−−−+ +−−−−−−−−−−−−−−−−+
// | 1 0 . 1 . 1 . 1 | | 1 0 . 1 . 1 . 2 |
// +−−−−−−−−−−−−−−−−+ +−−−−−−−−−−−−−−−−+
// | p oin t−to−p oi n t | | p oin t−to−p oi n t |
// +−−−−−−−−−−−−−−−−+ +−−−−−−−−−−−−−−−−+
// | |
// +−−−−−−−−−−−−−−−−−−−−−+
// 5 Mbps , 2 ms d el a y

#include "ns3/core-module.h" // core simulator functions
#include "ns3/network-module.h" // b a si c ne tw o rkin g c l a s s e s : nodes , d e vi c e s , e t c .
#include "ns3/point-to-point-module.h" // p oin t−to−p oi n t f u n c t i o n s
#include "ns3/internet-module.h" // i n t e r n e t s t a c k l o g i c
#include "ns3/applications-module.h" // a p p l i c a t i o n l a y e r : g e n e r a t e s / consumes data .

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ( "OurFirstSimulation" ) ;

//#include "ns3/core-module.h"

//NS_LOG_COMPONENT_DEFINE ("HelloSimulator");

//using namespace ns3;

int
main (int argc, char *argv[])
{
  //NS_LOG_UNCOND ("Hello Simulator");

	NS_LOG_UNCOND ( "OurFirstSimulation" ) ;

	int m_id = 0;
	double m_time = 100.0;
	uint16_t m_sinkPort = 8080;


	// Parse command line arguments. Variables must be declared beforehand!
	CommandLine cmd ;
	cmd.AddValue ( "id", "Experiment ID, to customize output file [0]" , m_id ) ;
	cmd.AddValue ( "time", "Simulationtime [100 s]" , m_time ) ;
	cmd.AddValue ( "sinkPort", "Port the server will be listening to [8080]" , m_sinkPort ) ;
	cmd.Parse ( argc, argv ) ;

	// Create the nodes
	NodeContainer nodes ;
	nodes.Create(2) ;

	// Create p2p helper with some configuration
	PointToPointHelper pointToPoint;
	pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps")) ;
	pointToPoint.SetChannelAttribute("Delay", StringValue("2ms") ) ;
	
	// Install devices on the nodes
	NetDeviceContainer devices;
	devices = pointToPoint.Install(nodes) ;
	
	// Error model. This is a smart−pointer! Instead of dealing with ∗’s and &’s, ns−3 uses Ptr <>.
	Ptr<RateErrorModel> em = CreateObject<RateErrorModel>( ) ;
	em−>SetAttribute("ErrorRate" , DoubleValue (0.00001));
	devices.Get (1)−>SetAttribute("ReceiveErrorModel" , PointerValue (em)) ;
	
	// TCP/IP stack
	InternetStackHelper stack;
	stack.Install(nodes);
	Ipv4AddressHelper addresses;
	addresses.SetBase( "10.1.1.0 " , "255.255.255.0" ) ;
	Ipv4InterfaceContainer interfaces = addresses.Assign(devices); // keeps track of device/address pairs

	/* // I n s t a l l s e r v e r a p p l i c a t i o n t o consume data
67 P ac ke tSin kHelpe r p a c k e t Si n kH el p e r ( ”ns3 : : TcpSocketFactory ” , I n e t S o c k e tA d d r e s s ( Ipv4Add ress : :
GetAny ( ) , m sinkPo r t ) ) ;
68 A p pli c a ti o n C o n t ai n e r sinkApps = p a c k e t Si n kH el p e r . I n s t a l l ( nodes . Get ( 1 ) ) ;
69 sinkApps . S t a r t ( Seconds ( 0 . ) ) ;
70 sinkApps . Stop ( Seconds ( 1 0 0 . ) ) ;
71
72 // S ou rce a p p l i c a t i o n . D e s ti n a ti o n = a d d r e s s o f node 1
73 BulkSendHelper bulkSendHelpe r ( ”ns3 : : TcpSocketFactory ” , I n e t S o c k e tA d d r e s s ( i n t e r f a c e s . GetAddress
( 1 ) , m sinkPo r t ) ) ;
74 bulkSendHelpe r . S e t A t t ri b u t e ( ”MaxBytes” , Uin te ge rV alue ( 1 0 0 0 0 0 0 0 0 0 ) ) ;
75 A p pli c a ti o n C o n t ai n e r bulkApps = bulkSendHelpe r . I n s t a l l ( nodes . Get ( 0 ) ) ;
76 bulkApps . S t a r t ( Seconds ( 1 . ) ) ;
77 bulkApps . Stop ( Seconds ( 9 9 . ) ) ;
78
79 // Run the sim ul a ti o n !
80 Sim ul a t o r : : Stop ( Seconds ( m time ) ) ;
81 Sim ul a t o r : : Run ( ) ;
82 Sim ul a t o r : : De s t roy ( ) ;*/
}
