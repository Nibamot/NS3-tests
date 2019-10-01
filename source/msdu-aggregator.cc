/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 MIRKO BANCHI
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

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MsduAggregator");

NS_OBJECT_ENSURE_REGISTERED (MsduAggregator);
std::map <Mac48Address, uint32_t> nooftxaggframesperstation = {};
std::map <Mac48Address, uint32_t> noofrxaggframesperstation = {};//hard-code follow
std::map <Mac48Address, uint32_t> noofrxaggpacketsperstation = {};
std::map <Mac48Address, uint32_t> basicpayloadsizeperstation = {};
std::map <Mac48Address, uint16_t> amsdusizeperstation = {{Mac48Address("00:00:00:00:00:01"), 3839},{Mac48Address("00:00:00:00:00:02"), 3839},{Mac48Address("00:00:00:00:00:03"), 3839},
                                                         {Mac48Address("00:00:00:00:00:04"), 3839}, {Mac48Address("00:00:00:00:00:05"), 3839}, {Mac48Address("00:00:00:00:00:06"), 3839},
                                                         {Mac48Address("00:00:00:00:00:07"), 3839}, {Mac48Address("00:00:00:00:00:08"), 3839}, {Mac48Address("00:00:00:00:00:09"), 3839},
                                                         {Mac48Address("00:00:00:00:00:0a"), 3839}};
uint nooftxaggframes = 0;
uint noofrxaggframes = 0;
uint nooftotalrxaggpkts = 0;
uint32_t noofframestx[10]={0};
uint32_t noofframesrx[10]={0};
uint32_t noofpacketsrx[10]={0};
uint pktsize = 0;

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


      if(dest == Mac48Address("00:00:00:00:00:01"))
      {
        noofframestx[0]++;
        nooftxaggframesperstation[dest]=noofframestx[0];
      }

      if(dest == Mac48Address("00:00:00:00:00:02"))
      {
        noofframestx[1]++;
        nooftxaggframesperstation[dest]=noofframestx[1];
      }

      if(dest == Mac48Address("00:00:00:00:00:03"))
      {
        noofframestx[2]++;
        nooftxaggframesperstation[dest]=noofframestx[2];
      }

      if(dest == Mac48Address("00:00:00:00:00:04"))
      {
        noofframestx[3]++;
        nooftxaggframesperstation[dest]=noofframestx[3];
      }

      if(dest == Mac48Address("00:00:00:00:00:05"))
      {
        noofframestx[4]++;
        nooftxaggframesperstation[dest]=noofframestx[4];
      }

      if(dest == Mac48Address("00:00:00:00:00:06"))
      {
        noofframestx[5]++;
        nooftxaggframesperstation[dest]=noofframestx[5];
      }

      if(dest == Mac48Address("00:00:00:00:00:07"))
      {
        noofframestx[6]++;
        nooftxaggframesperstation[dest]=noofframestx[6];
      }

      if(dest == Mac48Address("00:00:00:00:00:08"))
      {
        noofframestx[7]++;
        nooftxaggframesperstation[dest]=noofframestx[7];
      }

      if(dest == Mac48Address("00:00:00:00:00:09"))
      {
        noofframestx[8]++;
        nooftxaggframesperstation[dest]=noofframestx[8];
      }

      if(dest == Mac48Address("00:00:00:00:00:0a"))
      {
        noofframestx[9]++;
        nooftxaggframesperstation[dest]=noofframestx[9];
      }


      nooftxaggframes++;
      return true;
    }

  //printf("NOT Aggregating");
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
  nooftotalrxaggpkts+=set.size();

  if(dest == Mac48Address("00:00:00:00:00:01"))
  {
    noofframesrx[0]++;
    noofrxaggframesperstation[dest]=noofframesrx[0];
    noofpacketsrx[0]+=set.size();
    noofrxaggpacketsperstation[dest]=noofpacketsrx[0];
  }

  if(dest == Mac48Address("00:00:00:00:00:02"))
  {
    noofframesrx[1]++;
    noofrxaggframesperstation[dest]=noofframestx[1];
    noofpacketsrx[1]+=set.size();
    noofrxaggpacketsperstation[dest]=noofpacketsrx[1];
  }

  if(dest == Mac48Address("00:00:00:00:00:03"))
  {
    noofframesrx[2]++;
    noofrxaggframesperstation[dest]=noofframesrx[2];
    noofpacketsrx[2]+=set.size();
    noofrxaggpacketsperstation[dest]=noofpacketsrx[2];
  }

  if(dest == Mac48Address("00:00:00:00:00:04"))
  {
    noofframesrx[3]++;
    noofrxaggframesperstation[dest]=noofframesrx[3];
    noofpacketsrx[3]+=set.size();
    noofrxaggpacketsperstation[dest]=noofpacketsrx[3];
  }

  if(dest == Mac48Address("00:00:00:00:00:05"))
  {
    noofframesrx[4]++;
    noofrxaggframesperstation[dest]=noofframesrx[4];
    noofpacketsrx[4]+=set.size();
    noofrxaggpacketsperstation[dest]=noofpacketsrx[4];
  }

  if(dest == Mac48Address("00:00:00:00:00:06"))
  {
    noofframesrx[5]++;
    noofrxaggframesperstation[dest]=noofframesrx[5];
    noofpacketsrx[5]+=set.size();
    noofrxaggpacketsperstation[dest]=noofpacketsrx[5];
  }

  if(dest == Mac48Address("00:00:00:00:00:07"))
  {
    noofframesrx[6]++;
    noofrxaggframesperstation[dest]=noofframesrx[6];
    noofpacketsrx[6]+=set.size();
    noofrxaggpacketsperstation[dest]=noofpacketsrx[6];
  }

  if(dest == Mac48Address("00:00:00:00:00:08"))
  {
    noofframesrx[7]++;
    noofrxaggframesperstation[dest]=noofframesrx[7];
    noofpacketsrx[7]+=set.size();
    noofrxaggpacketsperstation[dest]=noofpacketsrx[7];
  }

  if(dest == Mac48Address("00:00:00:00:00:09"))
  {
    noofframesrx[8]++;
    noofrxaggframesperstation[dest]=noofframesrx[8];
    noofpacketsrx[8]+=set.size();
    noofrxaggpacketsperstation[dest]=noofpacketsrx[8];
  }

  if(dest == Mac48Address("00:00:00:00:00:0a"))
  {
    noofframesrx[9]++;
    noofrxaggframesperstation[dest]=noofframesrx[9];
    noofpacketsrx[9]+=set.size();
    noofrxaggpacketsperstation[dest]=noofpacketsrx[9];
  }


  NS_LOG_DEBUG("No of A-MSDU "<<noofrxaggframes<<" and total number of aggregated pkts "<<nooftotalrxaggpkts<<'\n');
  return set;
}

//EDITED

std::map <Mac48Address, uint32_t> MsduAggregator::getnooftxaggframesperstation(void)
{
  return nooftxaggframesperstation;

}

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
