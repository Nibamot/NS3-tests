/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 MIRKO BANCHI
 *
 *  Edited for aggregation and minstrel tips: Abin Thomas <tom.abin789@gmail.com>
 *
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
 *
 * Author: Mirko Banchi <mk.banchi@gmail.com>
 */

#include "ns3/log.h"
#include "ns3/packet.h"
#include "msdu-aggregator.h"
#include "amsdu-subframe-header.h"
#include "ns3/globalvalues.h"
//uint nstamsdu;
//uint nsta;
uint nsta;
namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MsduAggregator");

NS_OBJECT_ENSURE_REGISTERED (MsduAggregator);

std::map <Mac48Address, uint32_t> noofrxaggframesperstation = {};

std::map <Mac48Address, uint32_t> noofrxaggpacketsperstation = {};

std::map <Mac48Address, uint32_t> basicpayloadsizeperstation = {};

std::map <Mac48Address, uint16_t> amsdusizeperstation = {};

uint nooftxaggframes = 0;
uint noofrxaggframes = 0;
uint nooftotalrxaggpkts = 0;

std::vector<uint32_t> noofframesrx;//10
std::vector<uint32_t> noofpacketsrx;//10

bool setmaparraymsdu = false;


TypeId
MsduAggregator::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MsduAggregator")
    .SetParent<Object> ()
    .SetGroupName ("Wifi")
    .AddConstructor<MsduAggregator> ()
  ;
  return tid;
}

MsduAggregator::MsduAggregator ()
{
  if (!setmaparraymsdu)
  {
      SetAllMapsArraysmsdu();
      setmaparraymsdu = true;

  }
}

void MsduAggregator::SetAllMapsArraysmsdu(void)
{
  using namespace std;
  cout<<nsta;
  int decimal = nsta;
  noofframesrx.resize(decimal);
  noofpacketsrx.resize(decimal);

 for (int i = 1; i <= decimal; i++)
 {
   stringstream my_ss;
   my_ss << hex << i;
   string res = my_ss.str();
   string addressmac, extramac;

   if (res.size()==1)
   {
     extramac = "00:00:00:00:00:0";
     addressmac = extramac+res;
   }
  else
  {
    res.insert(0,"0");
    extramac = "00:00:00:00:00:";
    addressmac = extramac+res;

  }

  int charsize = addressmac.size();
  char stringchar[charsize];
  strcpy(stringchar, addressmac.c_str());


   noofrxaggframesperstation.insert(std::pair<Mac48Address,uint32_t>(Mac48Address(stringchar),0));
   noofrxaggpacketsperstation.insert(std::pair<Mac48Address,uint32_t>(Mac48Address(stringchar),0));
   basicpayloadsizeperstation.insert(std::pair<Mac48Address,uint32_t>(Mac48Address(stringchar),0));//for forced mcs
   amsdusizeperstation.insert(std::pair<Mac48Address,uint16_t>(Mac48Address(stringchar),3839));
   cout<<"at 01 "<<stringchar<<"\n";
 }


}



MsduAggregator::~MsduAggregator ()
{
}

void
MsduAggregator::SetMaxAmsduSize (uint16_t maxSize)
{
  m_maxAmsduLength = maxSize;
}

uint16_t
MsduAggregator::GetMaxAmsduSize (void) const
{
  return m_maxAmsduLength;
}

void
MsduAggregator::setamsdusizeperstation (Mac48Address dstaddress, uint16_t aggsize)
{
  amsdusizeperstation[dstaddress] = aggsize;
}



bool
MsduAggregator::Aggregate (Ptr<const Packet> packet, Ptr<Packet> aggregatedPacket,
                           Mac48Address src, Mac48Address dest) const
{
  NS_LOG_FUNCTION (this);
  Ptr<Packet> currentPacket;
  AmsduSubframeHeader currentHdr;
  uint8_t padding = CalculatePadding (aggregatedPacket);
  uint32_t actualSize = aggregatedPacket->GetSize ();
  basicpayloadsizeperstation[dest] = packet->GetSize () - 36;

  //change the aggregation size per station as to be able to pass the values from the prompt.
  if ((14 + packet->GetSize () + actualSize + padding) <= amsdusizeperstation[dest])// <= GetMaxAmsduSize ()//edited hardcode amsdusizeperstation[dest]
    {
      NS_LOG_DEBUG("the agg size "<<amsdusizeperstation[dest]<<" for station "<<dest);
      if (padding)
        {
          Ptr<Packet> pad = Create<Packet> (padding);
          aggregatedPacket->AddAtEnd (pad);
        }
      currentHdr.SetDestinationAddr (dest);
      currentHdr.SetSourceAddr (src);
      currentHdr.SetLength (static_cast<uint16_t> (packet->GetSize ()));
      currentPacket = packet->Copy ();

      currentPacket->AddHeader (currentHdr);
      aggregatedPacket->AddAtEnd (currentPacket);


      nooftxaggframes++;
      return true;
    }

  return false;
}

uint8_t
MsduAggregator::CalculatePadding (Ptr<const Packet> packet) const
{
  return (4 - (packet->GetSize () % 4 )) % 4;
}

MsduAggregator::DeaggregatedMsdus
MsduAggregator::Deaggregate (Ptr<Packet> aggregatedPacket)
{
  NS_LOG_FUNCTION_NOARGS ();
  DeaggregatedMsdus set;
  AmsduSubframeHeader hdr;
  Ptr<Packet> extractedMsdu = Create<Packet> ();
  uint32_t maxSize = aggregatedPacket->GetSize ();
  uint16_t extractedLength;
  uint8_t padding;
  uint32_t deserialized = 0;
  noofrxaggframes++;
  //aggregatedPacket->RemoveHeader(hdr);




  Mac48Address dest;
  while (deserialized < maxSize)
    {
      deserialized += aggregatedPacket->RemoveHeader (hdr);
      dest = hdr.GetDestinationAddr();
      extractedLength = hdr.GetLength ();
      extractedMsdu = aggregatedPacket->CreateFragment (0, static_cast<uint32_t> (extractedLength));
      aggregatedPacket->RemoveAtStart (extractedLength);
      deserialized += extractedLength;
      padding = (4 - ((extractedLength + 14) % 4 )) % 4;

      if (padding > 0 && deserialized < maxSize)
        {
          aggregatedPacket->RemoveAtStart (padding);
          deserialized += padding;
        }

      NS_LOG_DEBUG("extractedLength "<<extractedLength<<" deserialized "<<deserialized<<'\n');

      std::pair<Ptr<Packet>, AmsduSubframeHeader> packetHdr (extractedMsdu, hdr);
      set.push_back (packetHdr);
    }



  NS_LOG_INFO ("Deaggreated A-MSDU: extracted " << set.size () << " MSDUs");

  using namespace std;
  int decimal = nsta;

  noofframesrx.resize(decimal);
  noofpacketsrx.resize(decimal);

 for (int i = 1; i <= decimal; i++)
 {
   stringstream my_ss;
   my_ss << hex << i;
   string res = my_ss.str();
   string addressmac, extramac;

   if (res.size()==1)
   {
     extramac = "00:00:00:00:00:0";
     addressmac = extramac+res;
   }
  else
  {
    res.insert(0,"0");
    extramac = "00:00:00:00:00:";
    addressmac = extramac+res;
  }
  int charsize = addressmac.size();
  char stringchar[charsize];
  strcpy(stringchar, addressmac.c_str());

  if(dest == Mac48Address(stringchar))
  {
    noofframesrx[i-1]++;
    noofrxaggframesperstation[dest]=noofframesrx[i-1];
    noofpacketsrx[i-1]+=set.size();
    noofrxaggpacketsperstation[dest]=noofpacketsrx[i-1];
  }
 }

  nooftotalrxaggpkts+=set.size();

  NS_LOG_DEBUG("No of A-MSDU "<<noofrxaggframes<<" and total number of aggregated pkts "<<nooftotalrxaggpkts<<'\n');
  return set;
}

//hard edited

/*std::map <Mac48Address, uint32_t> MsduAggregator::getnooftxaggframesperstation(void)
{
  return nooftxaggframesperstation;

}*/

std::map <Mac48Address, uint32_t> MsduAggregator::getnoofrxaggframesperstation(void)
{
  return noofrxaggframesperstation;

}

std::map <Mac48Address, uint32_t> MsduAggregator::getnoofrxaggpacketsperstation(void)
{
  return noofrxaggpacketsperstation;

}

std::map <Mac48Address, uint16_t> MsduAggregator::getamsdusizeperstation(void)
{
  return amsdusizeperstation;

}

uint MsduAggregator::getnooftxaggframes(void)
{

  return nooftxaggframes;
}

uint MsduAggregator::getnoofrxaggframes(void)
{
return noofrxaggframes;

}


uint MsduAggregator::getnooftotalrxaggpkts(void)
{
return nooftotalrxaggpkts;

}


std::map <Mac48Address, uint32_t> MsduAggregator::getbasicpayloadsizeperstation(void)
{

  return basicpayloadsizeperstation;
}
//getbasicpktsize
/*
uint MsduAggregator::getbasicpktsi
*/
} //namespace ns3
