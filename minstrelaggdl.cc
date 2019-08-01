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
#include "ns3/data-rate.h"
#include <random>
#include "ns3/flow-monitor-module.h"
#include "ns3/arp-l3-protocol.h"
#include "iostream"
#include "fstream"
#include <chrono>
#include <math.h>


using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimpleMpduAggregation");



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

  uint32_t payloadSize = 256; //bytes
  uint64_t simulationTime = 10; //seconds number of transmitted packets and in effect received packets vary with this
  double distance = 21; //meters 21 is reco
  bool enableRts = 0;
  bool enablePcap = 0;
  bool verifyResults = 0; //used for regression
  uint BE_MaxAmsduSize = 1230;//moded
  uint nsta = 1;
  uint trialno = 10;
  uint mcsvalue = 1;
  uint64_t datarate = 500000;

  //Add debugging components
  LogComponentEnable ("SimpleMpduAggregation", LOG_LEVEL_INFO);
  LogComponentEnable ("MinstrelHtWifiManager", LOG_LEVEL_INFO);


  CommandLine cmd;
  cmd.AddValue ("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue ("enableRts", "Enable or disable RTS/CTS", enableRts);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("distance", "Distance in meters between the station and the access point", distance);
  cmd.AddValue ("BE_MaxAmsduSize", "A-MSDU aggregation size", BE_MaxAmsduSize);
  cmd.AddValue ("enablePcap", "Enable/disable pcap file generation", enablePcap);
  cmd.AddValue ("verifyResults", "Enable/disable results verification at the end of the simulation", verifyResults);
  cmd.AddValue ("nsta", "How many stations connected to the AP", nsta);
  cmd.AddValue ("trialno", "Trial number", trialno);
  cmd.AddValue ("mcsvalue", "MCS Type:", mcsvalue);
  cmd.AddValue ("datarate", "DataRate:", datarate);
  cmd.Parse (argc, argv);



  //setting up stream for csv
  std::ofstream fileme;
  fileme.open("Output-Training-A"+std::to_string(nsta)+"payload"+std::to_string(payloadSize)+".csv",std::fstream::app);
  fileme << "Stations,#Trial Number, Number of stations, Positionx,Positiony,Positionz,Payload,Aggregation size,MCS,Bitrate(kbps),Throughput,Goodput,Tx Packets,Rx Packets, Packet Loss Ratio, Mean Delay(ms), Channel Utilization\n";



    //Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", enableRts ? StringValue ("0") : StringValue ("999999"));
    //Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold",UintegerValue (2346));
    //Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue(payloadSize));
    //Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue("6.5Mb/s"));
    NS_LOG_INFO("Creating nodes");

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create (nsta);
    NodeContainer wifiApNode;
    wifiApNode.Create (1);


//Yans wiFi channel helper has log distance propagation model built-in
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.SetChannel (channel.Create ());
    //phy.Set ("TxPowerStart", DoubleValue (100));
    //phy.Set ("TxPowerEnd", DoubleValue (100));
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
        phyrate = 150;
        break;


      default:
        wifi.SetRemoteStationManager ("ns3::MinstrelHtWifiManager");
        break;

    }
    NS_LOG_INFO("Setting MCS to "<<mcsvalue);
    NS_LOG_INFO("Trail No: "<<trialno);


    WifiMacHelper mac;



    NetDeviceContainer staDevices, apDeviceC;
    Ssid ssid;





    ssid = Ssid ("network");
    phy.Set ("ChannelNumber", UintegerValue (44));
    mac.SetType ("ns3::StaWifiMac",
                "Ssid", SsidValue (ssid));

    NS_LOG_INFO("setting A-MSDU size to "<< BE_MaxAmsduSize);

    staDevices = wifi.Install (phy, mac, wifiStaNodes);


    mac.SetType ("ns3::ApWifiMac",
                "Ssid", SsidValue (ssid),
                "EnableBeaconJitter", BooleanValue (false),
                "BE_MaxAmpduSize", UintegerValue (0),//not having 0 has default 65535
                "BE_MaxAmsduSize", UintegerValue (BE_MaxAmsduSize));//VO_MaxAmsduSize, VI_MaxAmsduSize, BK_MaxAmsduSize, BE_MaxAmsduSize

//Enable A-MSDU with the highest maximum size allowed by the standard (7935 bytes)
    apDeviceC = wifi.Install (phy, mac, wifiApNode.Get (0));
    NS_LOG_INFO("installed AP with "<< ssid);

  /* Setting mobility model */
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  //Set position for APs
    positionAlloc->Add (Vector (0.0, 0.0, 0.0));
    positionAlloc->Add (Vector (40.0, 0.0, 0.0));


    uint positionxarr[nsta];
    uint positionyarr[nsta];
    uint positionzarr[nsta];
  //Set position for STAs
/*
    for (uint ind = 0; ind<nsta;++ind)
    {
      const int range_from  = 1;
      const int range_to    = distance;
      std::random_device      rand_dev;
      std::mt19937                        generator(rand_dev());
      std::uniform_int_distribution<int>  distr(range_from, range_to);
      uint randx = distr(generator);
      uint randy = distr(generator);
      uint randz = 0;
      positionAlloc->Add (Vector (randx, randy, 0.0));
      positionxarr[ind] = randx;
      positionyarr[ind] = randy;
      positionzarr[ind] = randz;

      NS_LOG_INFO("distance between the station "<<ind<<" and The AP is ("<<randx<<","<<randy<<") mts as (X,Y) ");
    }
*/
    mobility.SetPositionAllocator (positionAlloc);
    mobility.Install (wifiApNode);
    mobility.Install (wifiStaNodes);

  /* Internet stack */
    InternetStackHelper stack;
    stack.Install (wifiApNode);
    stack.Install (wifiStaNodes);

    Ipv4AddressHelper address;

    /*if(trialind == 1 )
      {
        address.SetBase ("192.168.0.0", "255.255.225.0","0.0.0.1");
      }
    else
      { std:: string nstas = std::to_string(trialind*nsta+trialind*2);
        std:: string bases = "0.0.0.";
        const char * c = bases.c_str();
        bases += nstas;
        address.SetBase ("192.168.0.0", "255.255.225.0",Ipv4Address(c));
      }
      */

    address.SetBase ("192.168.3.0", "255.255.255.0");
    Ipv4InterfaceContainer StaInterfaces;
    NS_LOG_INFO("Going to assign IP");

    StaInterfaces = address.Assign (staDevices);
    address.NewAddress();

    Ipv4InterfaceContainer ApInterfaceC;
    ApInterfaceC = address.Assign (apDeviceC);

    ApplicationContainer sinkApplications, sourceApplications;
    uint32_t port = 9;
  //fileme<<"Device,IP Address\n";
  //fileme<<"AP,"<<wifiApNode.Get (0)->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal ()<<"\n";

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
          sourceApplications.Add (onOffHelper.Install (wifiApNode.Get (0)));
          PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", sinkSocket);
          sinkApplications.Add (packetSinkHelper.Install (wifiStaNodes.Get (index)));

       }
     Ipv4GlobalRoutingHelper::PopulateRoutingTables ();


     FlowMonitorHelper flowmon;
     Ptr<FlowMonitor> monitor = flowmon.InstallAll();
     monitor = flowmon.Install(wifiStaNodes);
     monitor = flowmon.Install(wifiApNode);

     sourceApplications.Start (Seconds (1.0));
     sinkApplications.Start (Seconds (0.0));
     sinkApplications.Stop (Seconds (simulationTime + 2));

     sourceApplications.Stop (Seconds (simulationTime + 1));



     if (enablePcap)
     {
       phy.EnablePcap ("AP_C", apDeviceC.Get (0));
       for(uint i=0;i<nsta;i++)
       {
         phy.EnablePcap("STA_"+std::to_string(i)+"mcs"+std::to_string(mcsvalue)+"aggsize"+std::to_string(BE_MaxAmsduSize),staDevices.Get(i));

       }

     }

     Simulator::Stop (Seconds (simulationTime + 1));
     Simulator::Run ();

     double goodput[nsta] = {0};

     NS_LOG_INFO("The number of sink applications are "<<sinkApplications.GetN ());

     for (uint32_t index = 0; index < sinkApplications.GetN (); ++index)
     {
       uint64_t totalbytesThrough = DynamicCast<PacketSink> (sinkApplications.Get (index))->GetTotalRx ();
       NS_LOG_INFO("the number of bytes received by station without IP and UDP header"<<index<<" is "<<totalbytesThrough);//packets minus the ip and udp header overhead
       goodput[index] = ((totalbytesThrough * 8.0 ) /  simulationTime/(1024*1024));

       NS_LOG_INFO("the goodput of station "<<index<<" is "<<goodput[index]<<"Mb");
     }


     monitor->CheckForLostPackets ();
     Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
     std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

     uint staId= 0;
     for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
     {
       if(staId<nsta)
       {
         Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
         std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
         std::cout << "The distance of station "<<staId<<" from AP is ("<<std::to_string(positionxarr[staId])<<","<<
         std::to_string(positionyarr[staId])<<","<<std::to_string(positionzarr[staId])<<") "
         <<std::to_string(distfromAP(positionxarr[staId],positionyarr[staId], positionzarr[staId]))<<"mts\n";
         std::cout << "  Tx Bytes:   " << (i->second.txBytes) << "\n";
         std::cout << "  Rx Bytes:   " << (i->second.rxBytes) << "\n";
         double txpkts=  (i->second.txPackets);
         std::cout << "  Tx Packets:   " << txpkts << "\n";
         double rxpkts =  (i->second.rxPackets);
         std::cout << "  Rx Packets:   " << rxpkts << "\n";
         double throughput = i->second.rxBytes * 8.0 /  (1024*1024);
         std::cout << "  Throughput: " << throughput << " Mb\n";
         double gdput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / (1024*1024);
         std::cout << "  Goodput: " << gdput << " Mbps\n";
         double channelutil = goodput[staId] / phyrate;
         std::cout << "  Channel Utilization: " << channelutil<< " \n";
         double pktlossratio = (i->second.txPackets - i->second.rxPackets)*100/(double)i->second.txPackets;
         std::cout << "  Packet Loss Ratio: " << pktlossratio<< " \n";
         std::cout << "  Packet Dropped: " << (i->second.txPackets - i->second.rxPackets)  << "\n";
         double meandelay =  i->second.delaySum.GetSeconds()*1000/i->second.rxPackets;
         std::cout << "  mean Delay: " <<meandelay << " ms\n";
         std::cout << "  mean Jitter: " << i->second.jitterSum.GetSeconds()*1000/(i->second.rxPackets - 1) << " ms\n";
         std::cout << "  mean Hop count: " << 1 + i->second.timesForwarded/(double)i->second.rxPackets << "\n";

         fileme<<"Station "<<staId<<","<<trialno<<","<<nsta<<","<<positionxarr[staId]<<","<<positionyarr[staId]<<","<<positionzarr[staId]<<","<<payloadSize
            <<","<<BE_MaxAmsduSize<<","<<mcsvalue<<","<<datarate<<","<<throughput<<","<<goodput[staId]<<","<<txpkts<<","<<rxpkts<<","<<pktlossratio
            <<","<<meandelay<<","<<channelutil<<"\n";

        staId++;
      }

    }

    // ***** MAC Frame Error Rate **** //

    WifiRemoteStationInfo wifiRemSt ;
    NS_LOG_INFO ("Frame Error Rate = " << wifiRemSt.GetFrameErrorRate());

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
