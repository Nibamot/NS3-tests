/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the NU General Public License version 2 as
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
 *
 *
 */

#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/data-rate.h"
#include <random>
#include "ns3/flow-monitor-module.h"
#include "ns3/arp-l3-protocol.h"
#include "iostream"
#include "fstream"
#include <chrono>
#include <math.h>
#include "ns3/globalvalues.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimpleMpduAggregation");

uint nsta;

Ptr<UdpServer> udpapp;
uint64_t lastTotalRx = 0;


double distfromAP(double x, double y, double z)
{
  double sqdist = pow(x,2) + pow(y,2) + pow(z,2);
  return sqrt(sqdist);
}


int main (int argc, char *argv[])
{
  using namespace std::chrono;

// Use auto keyword to avoid typing long
// type definitions to get the timepoint
// at this instant use function now()
  auto start = high_resolution_clock::now();

  uint32_t payloadSize = 200; //bytes
  uint64_t simulationTime = 14; //seconds number of transmitted packets and in effect received packets vary with this
  double distance = 35; //meters 21 is reco
  bool enableRts = 0;
  bool enablePcap = 0;
  bool verifyResults = 0; //used for regression
  ::nsta = 5;
  uint BE_MaxAmsduSize = 3839;//moded
  uint trialno = 1;
  uint mcsvalue = 20;
  uint64_t datarate = 4000000;
  //bool ns3val = true;


  //Add debugging components
  LogComponentEnable ("SimpleMpduAggregation", LOG_LEVEL_INFO);
  //LogComponentEnable ("MinstrelHtWifiManager", LOG_LEVEL_INFO);
  //LogComponentEnable ("BlockAckAgreement", LOG_LEVEL_INFO);
  //LogComponentEnable ("MsduAggregator", LOG_LEVEL_INFO);
  //LogComponentEnable ("QosTxop", LOG_LEVEL_INFO);
  //  LogComponentEnable ("WifiPhy", LOG_LEVEL_INFO);
  //LogComponentEnable ("MacLow", LOG_LEVEL_INFO);
  //LogComponentEnable ("QosTxop", LOG_LEVEL_INFO);
  //LogComponentEnable ("FlowMonitor", LOG_LEVEL_INFO);
  //LogComponentEnable ("PropagationLossModel", LOG_LEVEL_INFO);
  //LogComponentEnable ("RegularWifiMac", LOG_LEVEL_INFO);
  //LogComponentEnable ("ApWifiMac", LOG_LEVEL_INFO);

  CommandLine cmd;
  cmd.AddValue ("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue ("enableRts", "Enable or disable RTS/CTS", enableRts);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("distance", "Distance in meters between the station and the access point", distance);
  cmd.AddValue ("BE_MaxAmsduSize", "A-MSDU aggregation size", BE_MaxAmsduSize);
  cmd.AddValue ("enablePcap", "Enable/disable pcap file generation", enablePcap);
  cmd.AddValue ("nsta", "How many stations connected to the AP", nsta);
  cmd.AddValue ("trialno", "Trial number", trialno);
  cmd.AddValue ("mcsvalue", "MCS Type:", mcsvalue);
  cmd.AddValue ("datarate", "DataRate:", datarate);
  cmd.Parse (argc, argv);



  //setting up stream for csv
  std::ofstream fileme;
  fileme.open("WalkRandomMAXaggregation.csv",std::fstream::app);
  fileme << "Stations,#Trial Number, Number of stations, Payload,Aggregation size,MostUsed AggregationSize, MCS, MostChosenMCS,Bitrate(bps),Throughput,Goodput,Tx Packets,Rx Packets, Aggregated frames, Aggregated packets, retransmitbytes, Packet Loss Ratio, SNR (dB), Mean Delay(ms), Channel Utilization\n";




    NS_LOG_INFO("Creating nodes");

    NodeContainer wifiStaNodes, wifistanodesup;
    wifiStaNodes.Create (nsta);
    wifistanodesup.Create (2);
    NodeContainer wifiApNode;
    wifiApNode.Create (1);


//Yans wiFi channel helper has log distance propagation model built-in
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.SetChannel (channel.Create ());
    //phy.Set ("TxPowerStart", DoubleValue (1));
    //phy.Set ("TxPowerEnd", DoubleValue (1));
    //for mcs beyond 8 and beyond



    if (mcsvalue >=8)
    {
      phy.Set ("ShortGuardEnabled", BooleanValue (false));
      phy.Set ("Antennas", UintegerValue (2));
      phy.Set ("MaxSupportedTxSpatialStreams", UintegerValue (2));
      phy.Set ("MaxSupportedRxSpatialStreams", UintegerValue (2));
    }

    else
    {
      phy.Set ("ShortGuardEnabled", BooleanValue (false));

    }
    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ);

    double phyrate;
  //Choose the MCS scheme
    switch(mcsvalue)// The required MCS value can be chosen from wifi-spectrum-saturation-example.cc
    {

      case 0:
        wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("HtMcs0"), "ControlMode", StringValue ("HtMcs0"));
        phyrate = 6.5;
        break;

      case 1:
        wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("HtMcs1"), "ControlMode", StringValue ("HtMcs1"));
        phyrate = 13;
        break;

      case 2:
        wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("HtMcs2"), "ControlMode", StringValue ("HtMcs2"));
        phyrate = 19.5;
        break;

      case 3:
        wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("HtMcs3"), "ControlMode", StringValue ("HtMcs3"));
        phyrate = 26;
        break;

      case 4:
        wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("HtMcs4"), "ControlMode", StringValue ("HtMcs4"));
        phyrate = 39;
        break;

      case 5:
        wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("HtMcs5"), "ControlMode", StringValue ("HtMcs5"));
        phyrate = 52;
        break;

      case 6:
        wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("HtMcs6"), "ControlMode", StringValue ("HtMcs6"));
        phyrate = 58.5;
        break;

      case 7:
        wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("HtMcs7"), "ControlMode", StringValue ("HtMcs7"));
        phyrate = 65;
        break;

      case 8:
        wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("HtMcs8"), "ControlMode", StringValue ("HtMcs8"));
        phyrate = 13;
        break;

      case 9:
        wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("HtMcs9"), "ControlMode", StringValue ("HtMcs9"));
        phyrate = 26;
        break;

      case 10:
        wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("HtMcs10"), "ControlMode", StringValue ("HtMcs10"));
        phyrate = 39;
        break;

      case 11:
        wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("HtMcs11"), "ControlMode", StringValue ("HtMcs11"));
        phyrate = 52;
        break;

      case 12:
        wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("HtMcs12"), "ControlMode", StringValue ("HtMcs12"));
        phyrate = 78;
        break;

      case 13:
        wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("HtMcs13"), "ControlMode", StringValue ("HtMcs13"));
        phyrate = 104;
        break;

      case 14:
        wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("HtMcs14"), "ControlMode", StringValue ("HtMcs14"));
        phyrate = 117;
        break;

      case 15:
        wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("HtMcs15"), "ControlMode", StringValue ("HtMcs0"));
        phyrate = 130;
        break;


      default:
        wifi.SetRemoteStationManager ("ns3::MinstrelHtWifiManager");
        break;

    }
    NS_LOG_INFO("Setting MCS to "<<mcsvalue);
    NS_LOG_INFO("Trail No: "<<trialno);


    WifiMacHelper mac;



    NetDeviceContainer staDevices, staDevicesup, apDeviceC;
    Ssid ssid;


    ssid = Ssid ("network");
    phy.Set ("ChannelNumber", UintegerValue (36));
    mac.SetType ("ns3::StaWifiMac",
                "Ssid", SsidValue (ssid));

    //NS_LOG_INFO("setting A-MSDU size to "<< BE_MaxAmsduSize);

    staDevices = wifi.Install (phy, mac, wifiStaNodes);
    staDevicesup = wifi.Install (phy, mac, wifistanodesup);

    mac.SetType ("ns3::ApWifiMac",
                "Ssid", SsidValue (ssid),
                "EnableBeaconJitter", BooleanValue (false),
                "BE_MaxAmpduSize", UintegerValue (0),//not having 0 has default 65535
                "BE_MaxAmsduSize", UintegerValue (BE_MaxAmsduSize));//VO_MaxAmsduSize, VI_MaxAmsduSize, BK_MaxAmsduSize, BE_MaxAmsduSize

//Enable A-MSDU with the highest maximum size allowed by the standard (7935 bytes)
    apDeviceC = wifi.Install (phy, mac, wifiApNode.Get (0));
    NS_LOG_INFO("installed AP with "<< ssid);

    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (20));
  /* Setting mobility model */
    MobilityHelper mobility;
    //just add the nodes in the order as needed. stas in this case is mobile rest static
    mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                   "X", StringValue ("0.0"),
                                   "Y", StringValue ("0.0"),
                                   "Rho", StringValue ("ns3::UniformRandomVariable[Min=0|Max=30]"));


    mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                               "Time", StringValue ("0.1s"),
                               "Speed", StringValue ("ns3::UniformRandomVariable[Min=2.0|Max=50.0]"),//ns3::UniformRandomVariable[Min=2.0|Max=4.0]//ns3::ConstantRandomVariable[Constant=40.0]
                               "Bounds", RectangleValue (Rectangle (-75, 75, -75, 75)));
    mobility.Install (wifiStaNodes);

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  //Set position for APs
    positionAlloc->Add (Vector (0.0, 0.0, 0.0));
    //positionAlloc->Add (Vector (distance, 0.0, 0.0));



    AnimationInterface::SetConstantPosition (wifiApNode.Get(0), 0, 0);
  //Set position for STAs


    positionAlloc->Add (Vector (5.0, 0.0, 0.0));
    positionAlloc->Add (Vector (-5.0, 0.0, 0.0));

    mobility.SetPositionAllocator (positionAlloc);
    mobility.Install (wifiApNode);
    //mobility.Install (wifiStaNodes);
    mobility.Install (wifistanodesup);


  /* Internet stack */
    InternetStackHelper stack;
    stack.Install (wifiApNode);
    stack.Install (wifiStaNodes);
    stack.Install (wifistanodesup);

    Ipv4AddressHelper address;


    address.SetBase ("192.168.3.0", "255.255.255.0");
    Ipv4InterfaceContainer StaInterfaces, StaInterfacesup;
    NS_LOG_INFO("Going to assign IP");

    StaInterfacesup = address.Assign (staDevicesup);
    address.NewAddress();
    StaInterfaces = address.Assign (staDevices);
    address.NewAddress();


    Ipv4InterfaceContainer ApInterfaceC;
    ApInterfaceC = address.Assign (apDeviceC);

    ApplicationContainer sinkApplicationssta, sourceApplicationsAp, sinkApplicationAp, sourceApplicationssta;
    uint32_t port = 9;


    for (uint32_t index = 0; index < nsta; ++index)
      {
          auto ipv4 = wifiStaNodes.Get (index)->GetObject<Ipv4> ();
          const auto address = ipv4->GetAddress (1, 0).GetLocal ();
          NS_LOG_INFO(wifiStaNodes.Get (index)->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal ());
          //fileme<<"Station "<<index<<","<<address<<"\n";
          InetSocketAddress sinkSocket (address, port);
          OnOffHelper onOffHelper ("ns3::UdpSocketFactory", sinkSocket);
          onOffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
          onOffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));//duty cycle for on and off of the application data
          onOffHelper.SetAttribute ("DataRate", DataRateValue (datarate));
          onOffHelper.SetAttribute ("PacketSize", UintegerValue (payloadSize)); //bytes
          //onOffHelper.SetAttribute ("MaxBytes", UintegerValue (datarate/8*simulationTime)); //bytes
          sourceApplicationsAp.Add (onOffHelper.Install (wifiApNode.Get (0)));
          PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", sinkSocket);
          sinkApplicationssta.Add (packetSinkHelper.Install (wifiStaNodes.Get (index)));

       }

       //uplink 2stations
           auto ipv4 = wifiApNode.Get (0)->GetObject<Ipv4> ();
           const auto addressup = ipv4->GetAddress (1, 0).GetLocal ();
           InetSocketAddress sinkSocketup (addressup, port);
           OnOffHelper onOffHelperup ("ns3::UdpSocketFactory", sinkSocketup);
           onOffHelperup.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
           onOffHelperup.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));//duty cycle for on and off of the application data
           onOffHelperup.SetAttribute ("DataRate", DataRateValue (1000000));//bits
           onOffHelperup.SetAttribute ("PacketSize", UintegerValue (payloadSize)); //bytes
           //onOffHelper.SetAttribute ("MaxBytes", UintegerValue (datarate/8*simulationTime)); //bytes
           sourceApplicationssta.Add (onOffHelperup.Install (wifistanodesup.Get (0)));
           sourceApplicationssta.Add (onOffHelperup.Install (wifistanodesup.Get (1)));
           PacketSinkHelper packetSinkHelperup ("ns3::UdpSocketFactory", sinkSocketup);
           sinkApplicationAp.Add (packetSinkHelperup.Install (wifiApNode.Get (0)));

     Ipv4GlobalRoutingHelper::PopulateRoutingTables ();


     FlowMonitorHelper flowmon;
     Ptr<FlowMonitor> monitor = flowmon.InstallAll();
     monitor = flowmon.Install(wifiStaNodes);
     monitor = flowmon.Install(wifiApNode);
     sourceApplicationsAp.Start (Seconds (1.0));
     sourceApplicationssta.Start (Seconds (1.00001));
     sinkApplicationssta.Start (Seconds (0.0));
     sinkApplicationAp.Start (Seconds (0.0));
     sinkApplicationAp.Stop (Seconds (simulationTime+1));
     sourceApplicationssta.Stop (Seconds (simulationTime+1));
     sinkApplicationssta.Stop (Seconds (simulationTime+1));
     sourceApplicationsAp.Stop (Seconds (simulationTime+1));



     if (enablePcap)
     {
       phy.EnablePcap ("AP_MCS"+std::to_string(mcsvalue)+"payload"+std::to_string(payloadSize), apDeviceC.Get (0));
     }

     Simulator::Stop (Seconds (simulationTime + 1));
     AnimationInterface anim ("walkanimationminstrel.xml"); // Mandatory
     for (uint32_t i = 0; i < wifiStaNodes.GetN (); ++i)
       {
         anim.UpdateNodeDescription (wifiStaNodes.Get (i), "STA"); // Optional
         anim.UpdateNodeColor (wifiStaNodes.Get (i), 255, 0, 0); // Optional
       }
    for (uint32_t i = 0; i < wifistanodesup.GetN (); ++i)
      {
         anim.UpdateNodeDescription (wifistanodesup.Get (i), "STA"); // Optional
         anim.UpdateNodeColor (wifistanodesup.Get (i), 255, 25, 0); // Optional
      }
     for (uint32_t i = 0; i < wifiApNode.GetN (); ++i)
       {
         anim.UpdateNodeDescription (wifiApNode.Get (i), "AP"); // Optional
         anim.UpdateNodeColor (wifiApNode.Get (i), 0, 255, 0); // Optional
       }
     Simulator::Run ();
     //monitor->SerializeToXmlFile("NameOfFile.xml", true, true);


     double goodput[nsta+2] = {0};

     NS_LOG_INFO("The number of sink applications are "<<sinkApplicationssta.GetN ());
     //goodput[0] = (DynamicCast<PacketSink> (sinkApplicationAp.Get (index))->GetTotalRx ())*8*0.5/simulationTime/(1024*1024);
     //goodput []

     for (uint32_t index = 0; index < sinkApplicationssta.GetN (); ++index)
     {

       uint64_t totalbytesThrough = DynamicCast<PacketSink> (sinkApplicationssta.Get (index))->GetTotalRx ();
       NS_LOG_INFO("the number of bytes received by station without IP and UDP header"<<index<<" is "<<totalbytesThrough);//packets minus the ip and udp header overhead
       if (datarate>1000000)
       {goodput[index] = ((totalbytesThrough * 8.0 ) /  (1024*1024*simulationTime));}
       else
       {goodput[index+2] = ((totalbytesThrough * 8.0 ) /  (1024*1024*simulationTime));}
       NS_LOG_INFO("the goodput of station "<<index<<" is "<<goodput[index]<<"Mbps");
     }



     monitor->CheckForLostPackets ();
     Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
     std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

     uint staId= 0;

     std::map<Mac48Address,double> fixedsnrmap = MacLow::fixedsnrperstation();
     std::map<Mac48Address,uint32_t> nooftxaggframesperstation = QosTxop::getnooftxaggframesperstation();
     std::map<Mac48Address,uint32_t> noofrxaggframesperstation = MsduAggregator::getnoofrxaggframesperstation();
     std::map<Mac48Address,uint32_t> noofrxaggpacketsperstation = MsduAggregator::getnoofrxaggpacketsperstation();
     //original mapping mac to ip (check)
     std::map<Mac48Address,double> ::iterator it;
     double snrarray[nsta];


     for(it = fixedsnrmap.begin(); it != fixedsnrmap.end(); it++)
     {
       if(staId<nsta)
       {
       std::cout << "The station MAC "<<(it->first)<<" the snr recorded"<<it->second<<"\n";// << " " << it->second.first << " " <<it->second.second << "\n";
       snrarray[staId] = it->second;
       staId++;
      }
     }



     uint32_t framesarraytx[nsta];
     staId = 0;
     std::map<Mac48Address,uint32_t> ::iterator ita;
     for(ita = nooftxaggframesperstation.begin(); ita != nooftxaggframesperstation.end(); ita++)
     {
       if(staId<nsta)
       {
       //std::cout << "The station MAC "<<(ita->first)<<" number of frames aggregated and sent"<<ita->second<<"\n";// << " " << it->second.first << " " <<it->second.second << "\n";
       framesarraytx[staId] = ita->second;
       staId++;
      }
     }


     uint32_t framesarrayrx[nsta];
     staId = 0;
     std::map<Mac48Address,uint32_t> ::iterator itb;
     for(itb = noofrxaggframesperstation.begin(); itb != noofrxaggframesperstation.end(); itb++)
     {
       if(staId<nsta)
       {
       //std::cout << "The station MAC "<<(itb->first)<<" number of frames aggregated and received"<<itb->second<<"\n";// << " " << it->second.first << " " <<it->second.second << "\n";
       framesarrayrx[staId] = itb->second;
       staId++;
      }
     }

     uint32_t packetsarrayrx[nsta];
     staId = 0;
     std::map<Mac48Address,uint32_t> ::iterator itc;
     for(itc = noofrxaggpacketsperstation.begin(); itc != noofrxaggpacketsperstation.end(); itc++)
     {
       if(staId<nsta)
       {
       //std::cout << "The station MAC "<<(itc->first)<<" number of packets aggregated and received"<<itc->second<<"\n";// << " " << it->second.first << " " <<it->second.second << "\n";
       packetsarrayrx[staId] = itc->second;
       staId++;
      }
     }
     std::map<Mac48Address, std::array<int, 4> > mostagglengthperstation = MinstrelHtWifiManager::GetMaxAggregationlengthused();
     uint32_t mostagglength[nsta] = {0}, mind[4] = {550, 1024, 2048, 3839}, mindex = 0;
     int maxl = 0;
     staId = 0;
     std::map<Mac48Address, std::array<int, 4> > ::iterator itmostagg;
     for(itmostagg = mostagglengthperstation.begin(); itmostagg != mostagglengthperstation.end(); itmostagg++)
     {
       if(staId<nsta)
       {
        while(mindex < 4)
         {
           if(maxl < itmostagg->second[mindex])
           {
             mostagglength[staId] = mind[mindex]/*itmostagg->second[mindex]*/;
             maxl = itmostagg->second[mindex];
             //std::cout<<"For station:"<<staId<<" "<<mind[mindex]<<" is chosen "<< itmostagg->second[mindex]<<" times\n";
           }


           mindex++;
         }
         //std::cout<<"For station:"<<staId<<" "<<mostagglength[staId]<<" is the most chosen\n";
         staId++;
         maxl = 0;
         mindex = 0;
      }
     }

     staId = 0;

     std::map<Mac48Address, std::array<int, 8> > mostmcsperstation = MinstrelHtWifiManager::GetMaxMcsused();
     uint32_t mostmcsused[nsta] = {0};
     maxl = 0;
     mindex = 0;
     std::map<Mac48Address, std::array<int, 8> > ::iterator itmostmcs;
     for(itmostmcs = mostmcsperstation.begin(); itmostmcs != mostmcsperstation.end(); itmostmcs++)
     {
       if(staId<nsta)
       {
        while(mindex < 8)
         {
           if(maxl < itmostmcs->second[mindex])
           {
             mostmcsused[staId] = mindex;
             maxl = itmostmcs->second[mindex];
             std::cout<<"For station:"<<staId<<" "<<mindex<<" is chosen "<< itmostmcs->second[mindex]<<" times\n";
           }


           mindex++;
         }
         //std::cout<<"For station:"<<staId<<" "<<mostagglength[staId]<<" is the most chosen\n";
         staId++;
         maxl = 0;
         mindex = 0;
      }
     }


     staId = 0;

     for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
     {
       if (datarate>1000000)
       {
         if(staId<nsta)
       {
         Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
         std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
         std::cout << "  Tx Bytes:   " << (i->second.txBytes) << "\n";
         std::cout << "  Rx Bytes:   " << (i->second.rxBytes) << "\n";
         double txpkts=  (i->second.txPackets);
         std::cout << "  Tx Packets:   " << txpkts << "\n";
         double rxpkts =  (i->second.rxPackets);
         std::cout << "  Rx Packets:   " << rxpkts << "\n";
         std::cout << "Number of aggregated frames transmitted: "<< framesarraytx[staId]<<"\n";
         std::cout << "Number of aggregated frames received: "<< framesarrayrx[staId]<<"\n";
         std::cout << "Number of total aggregated packets received: "<< packetsarrayrx[staId]<<"\n";
         double throughput = i->second.rxBytes * 8.0 /  (1024*1024);
         std::cout << "  Throughput: " << throughput << " Mb\n";
         double gdput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / (1024*1024);
         std::cout << "  Goodput: " << gdput << " Mbps\n";
         phyrate = 6.5;
         double channelutil = (goodput[staId] / phyrate)+0.0084154;
         std::cout << "  Channel Utilization: " << channelutil<< " \n";
         double pktlossratio = (i->second.txPackets - i->second.rxPackets)*100/(double)i->second.txPackets;
         std::cout << "  Packet Loss Ratio: " << pktlossratio<< " \n";
         std::cout << "  Packet Dropped: " << (i->second.txPackets - i->second.rxPackets)  << "\n";
         double meandelay =  i->second.delaySum.GetSeconds()*1000/i->second.rxPackets;
         std::cout << "  mean Delay: " <<meandelay << " ms\n";
         std::cout << "  mean Jitter: " << i->second.jitterSum.GetSeconds()*1000/(i->second.rxPackets - 1) << " ms\n";
         std::cout << "  mean Hop count: " << 1 + i->second.timesForwarded/(double)i->second.rxPackets << "\n";

         fileme<<"Station "<<staId<<","<<trialno<<","<<nsta<<","<<payloadSize
            <<","<<BE_MaxAmsduSize<<","<<mostagglength[staId]<<","<<mcsvalue<<","<<mostmcsused[staId]<<","<<datarate<<","<<throughput<<","<<goodput[staId]<<","<<txpkts<<","<<rxpkts<<","<<framesarrayrx[staId]<<","<<packetsarrayrx[staId]
            <<","<<QosTxop::getnoofretransmssionsbytes()<<","<<pktlossratio<<","<<snrarray[staId]<<","<<meandelay<<","<<channelutil<<"\n";

        staId++;
      }

       }
         else
         {
         if(staId<nsta+2)
       {
         Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
         std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
         std::cout << "  Tx Bytes:   " << (i->second.txBytes) << "\n";
         std::cout << "  Rx Bytes:   " << (i->second.rxBytes) << "\n";
         double txpkts=  (i->second.txPackets);
         std::cout << "  Tx Packets:   " << txpkts << "\n";
         double rxpkts =  (i->second.rxPackets);
         std::cout << "  Rx Packets:   " << rxpkts << "\n";
         std::cout << "Number of aggregated frames transmitted: "<< framesarraytx[staId]<<"\n";
         std::cout << "Number of aggregated frames received: "<< framesarrayrx[staId]<<"\n";
         std::cout << "Number of total aggregated packets received: "<< packetsarrayrx[staId]<<"\n";
         double throughput = i->second.rxBytes * 8.0 /  (1024*1024);
         std::cout << "  Throughput: " << throughput << " Mb\n";
         double gdput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / (1024*1024);
         std::cout << "  Goodput: " << gdput << " Mbps\n";
         phyrate = 6.5;
         double channelutil = (goodput[staId] / phyrate)+0.0084154;
         std::cout << "  Channel Utilization: " << channelutil<< " \n";
         double pktlossratio = (i->second.txPackets - i->second.rxPackets)*100/(double)i->second.txPackets;
         std::cout << "  Packet Loss Ratio: " << pktlossratio<< " \n";
         std::cout << "  Packet Dropped: " << (i->second.txPackets - i->second.rxPackets)  << "\n";
         double meandelay =  i->second.delaySum.GetSeconds()*1000/i->second.rxPackets;
         std::cout << "  mean Delay: " <<meandelay << " ms\n";
         std::cout << "  mean Jitter: " << i->second.jitterSum.GetSeconds()*1000/(i->second.rxPackets - 1) << " ms\n";
         std::cout << "  mean Hop count: " << 1 + i->second.timesForwarded/(double)i->second.rxPackets << "\n";
         if(staId>1){
         fileme<<"Station "<<staId<<","<<trialno<<","<<nsta<<","<<payloadSize
            <<","<<BE_MaxAmsduSize<<","<<mostagglength[staId]<<","<<mcsvalue<<","<<mostmcsused[staId]<<","<<datarate<<","<<throughput<<","<<goodput[staId]<<","<<txpkts<<","<<rxpkts<<","<<framesarrayrx[staId]<<","<<packetsarrayrx[staId]
            <<","<<QosTxop::getnoofretransmssionsbytes()<<","<<pktlossratio<<","<<snrarray[staId]<<","<<meandelay<<","<<channelutil<<"\n";
   }
        staId++;
      }
   }
    }


    NS_LOG_DEBUG("No of retransmissions "<<QosTxop::getnoofretransmssions());
    NS_LOG_DEBUG("/nNo of retransmissions in bytes"<<QosTxop::getnoofretransmssionsbytes());
    fileme<<"\n";


   fileme.close();
   Simulator::Destroy ();
   using namespace std::chrono;

// After function call
   auto stop = high_resolution_clock::now();
   auto duration = duration_cast<seconds>(stop - start );

// To get the value of duration use the count()
// member function on the duration object
   std::cout << "run duration: "  <<duration.count();

   return 0;


}
