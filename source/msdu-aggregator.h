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

#ifndef MSDU_AGGREGATOR_H
#define MSDU_AGGREGATOR_H

#include "ns3/object.h"

namespace ns3 {

class AmsduSubframeHeader;
class WifiMacHeader;
class Packet;

/**
 * \brief Aggregator used to construct A-MSDUs
 * \ingroup wifi
 */

class MsduAggregator : public Object
{
public:
  /// DeaggregatedMsdus typedef
  typedef std::list<std::pair<Ptr<Packet>, AmsduSubframeHeader> > DeaggregatedMsdus;
  /// DeaggregatedMsdusCI typedef
  typedef std::list<std::pair<Ptr<Packet>, AmsduSubframeHeader> >::const_iterator DeaggregatedMsdusCI;

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  MsduAggregator ();
  virtual ~MsduAggregator ();

  /**
   * Sets the maximum A-MSDU size in bytes.
   * Value 0 means that MSDU aggregation is disabled.
   *
   * \param maxSize the maximum A-MSDU size in bytes.
   */
  void SetMaxAmsduSize (uint16_t maxSize);
  /**
   * Returns the maximum A-MSDU size in bytes.
   * Value 0 means that MSDU aggregation is disabled.
   *
   * \return the maximum A-MSDU size in bytes.
   */
  uint16_t GetMaxAmsduSize (void) const;

  /**
   * Adds <i>packet</i> to <i>aggregatedPacket</i>. In concrete aggregator's implementation is
   * specified how and if <i>packet</i> can be added to <i>aggregatedPacket</i>. If <i>packet</i>
   * can be added returns true, false otherwise.
   *
   * \param packet the packet.
   * \param aggregatedPacket the aggregated packet.
   * \param src the source address.
   * \param dest the destination address
   * \return true if successful.
   */
  bool Aggregate (Ptr<const Packet> packet, Ptr<Packet> aggregatedPacket,
                  Mac48Address src, Mac48Address dest) const;

  //set all maps and arrays msdu aggregator
  void SetAllMapsArraysmsdu (void);
  /**
   *
   * \param aggregatedPacket the aggregated packet.
   * \returns DeaggregatedMsdus.
   */
  static DeaggregatedMsdus Deaggregate (Ptr<Packet> aggregatedPacket);

  //Get number of tx aggregated super packets
  static uint getnooftxaggframes(void);

  //Get number of rx aggregated super packets
  static uint getnoofrxaggframes(void);

  //Get number of rx aggregated sub-frames
  static uint getnooftotalrxaggpkts(void);

  /*//Get a map of station to the number of aggregated frames transmitted
  static std::map<Mac48Address, uint32_t> getnooftxaggframesperstation(void);//hard-code follow
*/

  //Get a map of station to the number of aggregated frames received
  static std::map<Mac48Address, uint32_t> getnoofrxaggframesperstation(void);


  //Get a map of station to the number of total aggregated packets received
  static std::map<Mac48Address, uint32_t> getnoofrxaggpacketsperstation(void);

  static std::map<Mac48Address, uint32_t> getbasicpayloadsizeperstation(void);

  static void setamsdusizeperstation(Mac48Address dstaddress, uint16_t aggsize);

  static std::map<Mac48Address, uint16_t> getamsdusizeperstation(void);

private:
  /**
   * Calculates how much padding must be added to the end of aggregated packet,
   * after that a new packet is added.
   * Each A-MSDU subframe is padded so that its length is multiple of 4 octets.
   *
   * \param packet
   *
   * \return the number of octets required for padding
   */
  uint8_t CalculatePadding (Ptr<const Packet> packet) const;

  uint16_t m_maxAmsduLength; ///< maximum AMSDU length
};

} //namespace ns3

#endif /* MSDU_AGGREGATOR_H */
