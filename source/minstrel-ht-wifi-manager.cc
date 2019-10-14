/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 Duy Nguyen
 * Copyright (c) 2015 Ghada Badawy
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
 * Authors: Duy Nguyen <duy@soe.ucsc.edu>
 *          Ghada Badawy <gbadawy@gmail.com>
 *          Matias Richart <mrichart@fing.edu.uy>
 *
 * Some Comments:
 *
 * 1) By default, Minstrel applies the multi-rate retry (the core of Minstrel
 *    algorithm). Otherwise, please use ConstantRateWifiManager instead.
 *
 * 2) Sampling is done differently from legacy Minstrel. Minstrel-HT tries
 * to sample all rates in all groups at least once and to avoid many
 * consecutive samplings.
 *
 * 3) Sample rate is tried only once, at first place of the MRR chain.
 *
 * reference: http://lwn.net/Articles/376765/
 */

#include <iomanip>
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/random-variable-stream.h"
#include "minstrel-ht-wifi-manager.h"
#include "wifi-mac.h"
#include "wifi-phy.h"
#include "msdu-aggregator.h"
#include "mac-low.h"
#include "chrono"
#include "thread"
#include <python2.7/Python.h>

#define Min(a,b) ((a < b) ? a : b)
#define Max(a,b) ((a > b) ? a : b)

NS_LOG_COMPONENT_DEFINE ("MinstrelHtWifiManager");

namespace ns3 {

std::map <uint, float> mcsphyrate = {{0,6.5},{1,13},{2,19.5},{3,26},{4,39},{5,52},{6,58.5},{7,65}};
std::map <Mac48Address, uint> addresslist = {{Mac48Address("00:00:00:00:00:01"), 0},{Mac48Address("00:00:00:00:00:02"), 0},{Mac48Address("00:00:00:00:00:03"), 0},
                                                         {Mac48Address("00:00:00:00:00:04"), 0}, {Mac48Address("00:00:00:00:00:05"), 0}, {Mac48Address("00:00:00:00:00:06"), 0},
                                                         {Mac48Address("00:00:00:00:00:07"), 0}, {Mac48Address("00:00:00:00:00:08"), 0}, {Mac48Address("00:00:00:00:00:09"), 0},
                                                         {Mac48Address("00:00:00:00:00:0a"), 0}};
std::map <Mac48Address, uint> addressstamap = {{Mac48Address("00:00:00:00:00:01"), 1},{Mac48Address("00:00:00:00:00:02"), 2},{Mac48Address("00:00:00:00:00:03"), 3},
                                                                                                                  {Mac48Address("00:00:00:00:00:04"), 4}, {Mac48Address("00:00:00:00:00:05"), 5}, {Mac48Address("00:00:00:00:00:06"), 6},
                                                                                                                  {Mac48Address("00:00:00:00:00:07"), 7}, {Mac48Address("00:00:00:00:00:08"), 8}, {Mac48Address("00:00:00:00:00:09"), 9},
                                                                                                                  {Mac48Address("00:00:00:00:00:0a"), 10}};

std::map <Mac48Address, uint16_t> lastmcsperaddress = {{Mac48Address("00:00:00:00:00:01"), 0},{Mac48Address("00:00:00:00:00:02"), 0},{Mac48Address("00:00:00:00:00:03"), 0},
                                             {Mac48Address("00:00:00:00:00:04"), 0}, {Mac48Address("00:00:00:00:00:05"), 0}, {Mac48Address("00:00:00:00:00:06"), 0},
                                             {Mac48Address("00:00:00:00:00:07"), 0}, {Mac48Address("00:00:00:00:00:08"), 0}, {Mac48Address("00:00:00:00:00:09"), 0},
                                             {Mac48Address("00:00:00:00:00:0a"), 0}};

std::map <int, float> mcs0agglengthexpth = {{550, 0},{1024, 0},{2048, 0},{3839, 0}};
std::map <int, float> mcs1agglengthexpth = {{550, 0},{1024, 0},{2048, 0},{3839, 0}};
std::map <int, float> mcs2agglengthexpth = {{550, 0},{1024, 0},{2048, 0},{3839, 0}};
std::map <int, float> mcs3agglengthexpth = {{550, 0},{1024, 0},{2048, 0},{3839, 0}};
std::map <int, float> mcs4agglengthexpth = {{550, 0},{1024, 0},{2048, 0},{3839, 0}};
std::map <int, float> mcs5agglengthexpth = {{550, 0},{1024, 0},{2048, 0},{3839, 0}};
std::map <int, float> mcs6agglengthexpth = {{550, 0},{1024, 0},{2048, 0},{3839, 0}};
std::map <int, float> mcs7agglengthexpth = {{550, 0},{1024, 0},{2048, 0},{3839, 0}};
std::map <int, float> genagglengthexpth = {};


float globalchanutil, globalchanutilns3, globalchanutilplatform;
bool ns3val = false;
int checker = 0;
std::map<Mac48Address, float> chanutil = {};
std::map<Mac48Address, float> successrations3 = {};
std::map<Mac48Address, float> successratioplatform = {};
std::map<Mac48Address, float> minstrel_throughput = {};
std::map<Mac48Address, float> last_attempt_bytes = {};
std::map<Mac48Address, double> historical_bytes = {};
std::map<Mac48Address, double> previous_historical_bytes = {};
std::map<Mac48Address, float> second_statistics = {};
std::map<Mac48Address, float> previous_statistics = {};
std::map <Mac48Address, uint16_t> previousaggregationlengthperstation= {};

//for banned counter
std::array<int, 4> sta1bannedcount = {0};
std::array<int, 4> sta2bannedcount = {0};
std::array<int, 4> sta3bannedcount = {0};
std::array<int, 4> sta4bannedcount = {0};
std::array<int, 4> sta5bannedcount = {0};
std::array<int, 4> sta6bannedcount = {0};
std::array<int, 4> sta7bannedcount = {0};
std::array<int, 4> sta8bannedcount = {0};
std::array<int, 4> sta9bannedcount = {0};
std::array<int, 4> sta10bannedcount = {0};

//map fr banned counter per station
std::map <Mac48Address, std::array<int, 4> > bannedagglengthcounterperstation = {{Mac48Address("00:00:00:00:00:01"), sta1bannedcount},{Mac48Address("00:00:00:00:00:02"), sta2bannedcount},{Mac48Address("00:00:00:00:00:03"), sta3bannedcount},
                                             {Mac48Address("00:00:00:00:00:04"), sta4bannedcount}, {Mac48Address("00:00:00:00:00:05"), sta5bannedcount}, {Mac48Address("00:00:00:00:00:06"), sta6bannedcount},
                                             {Mac48Address("00:00:00:00:00:07"), sta7bannedcount}, {Mac48Address("00:00:00:00:00:08"), sta8bannedcount}, {Mac48Address("00:00:00:00:00:09"), sta9bannedcount},
                                             {Mac48Address("00:00:00:00:00:0a"), sta10bannedcount}};

std::map <uint16_t, int> agglengthtoindexmap = {{550,0},{1024,1},{2048,2},{3839,3}};
uint len_1, len_2, len_3, len_4;

///MinstrelHtWifiRemoteStation structure
struct MinstrelHtWifiRemoteStation : MinstrelWifiRemoteStation
{
  uint8_t m_sampleGroup;     //!< The group that the sample rate belongs to.

  uint32_t m_sampleWait;      //!< How many transmission attempts to wait until a new sample.
  uint32_t m_sampleTries;     //!< Number of sample tries after waiting sampleWait.
  uint32_t m_sampleCount;     //!< Max number of samples per update interval.
  uint32_t m_numSamplesSlow;  //!< Number of times a slow rate was sampled.

  uint32_t m_avgAmpduLen;      //!< Average number of MPDUs in an A-MPDU.
  uint32_t m_ampduLen;         //!< Number of MPDUs in an A-MPDU.
  uint32_t m_ampduPacketCount; //!< Number of A-MPDUs transmitted.

  McsGroupData m_groupsTable;  //!< Table of groups with stats.
  bool m_isHt;                 //!< If the station is HT capable.

  std::ofstream m_statsFile;   //!< File where statistics table is written.
};

NS_OBJECT_ENSURE_REGISTERED (MinstrelHtWifiManager);

TypeId
MinstrelHtWifiManager::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MinstrelHtWifiManager")
    .SetParent<WifiRemoteStationManager> ()
    .AddConstructor<MinstrelHtWifiManager> ()
    .SetGroupName ("Wifi")
    .AddAttribute ("UpdateStatistics",
                   "The interval between updating statistics table ",
                   TimeValue (MilliSeconds (100)),
                   MakeTimeAccessor (&MinstrelHtWifiManager::m_updateStats),
                   MakeTimeChecker ())
    .AddAttribute ("LookAroundRate",
                   "The percentage to try other rates (for legacy Minstrel)",
                   UintegerValue (10),
                   MakeUintegerAccessor (&MinstrelHtWifiManager::m_lookAroundRate),
                   MakeUintegerChecker<uint8_t>(0, 100))
    .AddAttribute ("EWMA",
                   "EWMA level",
                   UintegerValue (75),
                   MakeUintegerAccessor (&MinstrelHtWifiManager::m_ewmaLevel),
                   MakeUintegerChecker<uint8_t>(0, 100))
    .AddAttribute ("SampleColumn",
                   "The number of columns used for sampling",
                   UintegerValue (10),
                   MakeUintegerAccessor (&MinstrelHtWifiManager::m_nSampleCol),
                   MakeUintegerChecker <uint8_t> ())
    .AddAttribute ("PacketLength",
                   "The packet length used for calculating mode TxTime",
                   UintegerValue (1200),
                   MakeUintegerAccessor (&MinstrelHtWifiManager::m_frameLength),
                   MakeUintegerChecker <uint32_t> ())
    .AddAttribute ("UseVhtOnly",
                   "Use only VHT MCSs (and not HT) when VHT is available",
                   BooleanValue (true),
                   MakeBooleanAccessor (&MinstrelHtWifiManager::m_useVhtOnly),
                   MakeBooleanChecker ())
    .AddAttribute ("PrintStats",
                   "Control the printing of the statistics table",
                   BooleanValue (true),
                   MakeBooleanAccessor (&MinstrelHtWifiManager::m_printStats),
                   MakeBooleanChecker ())
    .AddTraceSource ("Rate",
                     "Traced value for rate changes (b/s)",
                     MakeTraceSourceAccessor (&MinstrelHtWifiManager::m_currentRate),
                     "ns3::TracedValueCallback::Uint64")
  ;
  return tid;
}

MinstrelHtWifiManager::MinstrelHtWifiManager ()
  : m_numGroups (0),
    m_numRates (0),
    m_currentRate (0)
{
  NS_LOG_FUNCTION (this);
  m_uniformRandomVariable = CreateObject<UniformRandomVariable> ();
  /**
   *  Create the legacy Minstrel manager in case HT is not supported by the device
   *  or non-HT stations want to associate.
   */
  m_legacyManager = CreateObject<MinstrelWifiManager> ();
}

MinstrelHtWifiManager::~MinstrelHtWifiManager ()
{
  NS_LOG_FUNCTION (this);
  if (HasHtSupported ())
    {
      for (uint8_t i = 0; i < m_numGroups; i++)
        {
          m_minstrelGroups[i].ratesFirstMpduTxTimeTable.clear ();
          m_minstrelGroups[i].ratesTxTimeTable.clear ();
        }
    }
}

int64_t
MinstrelHtWifiManager::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  int64_t numStreamsAssigned = 0;
  m_uniformRandomVariable->SetStream (stream);
  numStreamsAssigned++;
  numStreamsAssigned += m_legacyManager->AssignStreams (stream);
  return numStreamsAssigned;
}

void
MinstrelHtWifiManager::SetupPhy (const Ptr<WifiPhy> phy)
{
  NS_LOG_FUNCTION (this << phy);
  // Setup phy for legacy manager.
  m_legacyManager->SetupPhy (phy);
  WifiRemoteStationManager::SetupPhy (phy);
}

void
MinstrelHtWifiManager::DoInitialize ()
{
  NS_LOG_FUNCTION (this);

  /**
   * Here we initialize m_minstrelGroups with all the possible groups.
   * If a group is not supported by the device, then it is marked as not supported.
   * Then, after all initializations are finished, we check actual support for each receiving station.
   */

  // Check if the device supports HT or VHT
  if (HasHtSupported () || HasVhtSupported ())
    {
      m_numGroups = MAX_SUPPORTED_STREAMS * MAX_HT_STREAM_GROUPS;
      m_numRates = MAX_HT_GROUP_RATES;

      if (HasVhtSupported ())
        {
          m_numGroups += MAX_SUPPORTED_STREAMS * MAX_VHT_STREAM_GROUPS;
          m_numRates = MAX_VHT_GROUP_RATES;
        }

      /**
       *  Initialize the groups array.
       *  The HT groups come first, then the VHT ones.
       *  Minstrel maintains different types of indexes:
       *  - A global continuous index, which identifies all rates within all groups, in [0, m_numGroups * m_numRates]
       *  - A groupId, which indexes a group in the array, in [0, m_numGroups]
       *  - A rateId, which identifies a rate within a group, in [0, m_numRates]
       *  - A deviceIndex, which indexes a MCS in the phy MCS array.
       *  - A mcsIndex, which indexes a MCS in the wifi-remote-station-manager supported MCSs array.
       */
      NS_LOG_DEBUG ("Initialize MCS Groups:");
      m_minstrelGroups = MinstrelMcsGroups (m_numGroups);

      // Initialize all HT groups
      for (uint16_t chWidth = 20; chWidth <= MAX_HT_WIDTH; chWidth *= 2)
        {
          for (uint8_t sgi = 0; sgi <= 1; sgi++)
            {
              for (uint8_t streams = 1; streams <= MAX_SUPPORTED_STREAMS; streams++)
                {
                  uint8_t groupId = GetHtGroupId (streams, sgi, chWidth);

                  m_minstrelGroups[groupId].streams = streams;
                  m_minstrelGroups[groupId].sgi = sgi;
                  m_minstrelGroups[groupId].chWidth = chWidth;
                  m_minstrelGroups[groupId].isVht = false;
                  m_minstrelGroups[groupId].isSupported = false;

                  // Check capabilities of the device
                  if (!(!GetPhy ()->GetShortGuardInterval () && m_minstrelGroups[groupId].sgi)                   ///Is SGI supported by the transmitter?
                      && (GetPhy ()->GetChannelWidth () >= m_minstrelGroups[groupId].chWidth)               ///Is channel width supported by the transmitter?
                      && (GetPhy ()->GetMaxSupportedTxSpatialStreams () >= m_minstrelGroups[groupId].streams))  ///Are streams supported by the transmitter?
                    {
                      m_minstrelGroups[groupId].isSupported = true;

                      // Calculate tx time for all rates of the group
                      WifiModeList htMcsList = GetHtDeviceMcsList ();
                      for (uint8_t i = 0; i < MAX_HT_GROUP_RATES; i++)
                        {
                          uint16_t deviceIndex = i + (m_minstrelGroups[groupId].streams - 1) * 8;
                          NS_LOG_DEBUG("Device index "<<deviceIndex);
                          WifiMode mode =  htMcsList[deviceIndex];
                          AddFirstMpduTxTime (groupId, mode, CalculateFirstMpduTxDuration (GetPhy (), streams, sgi, chWidth, mode));
                          AddMpduTxTime (groupId, mode, CalculateMpduTxDuration (GetPhy (), streams, sgi, chWidth, mode));
                        }
                      NS_LOG_DEBUG ("Initialized group " << +groupId << ": (" << +streams << "," << +sgi << "," << chWidth << ")");
                    }
                }
            }
        }

      if (HasVhtSupported ())
        {
          // Initialize all VHT groups
          for (uint16_t chWidth = 20; chWidth <= MAX_VHT_WIDTH; chWidth *= 2)
            {
              for (uint8_t sgi = 0; sgi <= 1; sgi++)
                {
                  for (uint8_t streams = 1; streams <= MAX_SUPPORTED_STREAMS; streams++)
                    {
                      uint8_t groupId = GetVhtGroupId (streams, sgi, chWidth);

                      m_minstrelGroups[groupId].streams = streams;
                      m_minstrelGroups[groupId].sgi = sgi;
                      m_minstrelGroups[groupId].chWidth = chWidth;
                      m_minstrelGroups[groupId].isVht = true;
                      m_minstrelGroups[groupId].isSupported = false;

                      // Check capabilities of the device
                      if (!(!GetPhy ()->GetShortGuardInterval () && m_minstrelGroups[groupId].sgi)                   ///Is SGI supported by the transmitter?
                          && (GetPhy ()->GetChannelWidth () >= m_minstrelGroups[groupId].chWidth)               ///Is channel width supported by the transmitter?
                          && (GetPhy ()->GetMaxSupportedTxSpatialStreams () >= m_minstrelGroups[groupId].streams))  ///Are streams supported by the transmitter?
                        {
                          m_minstrelGroups[groupId].isSupported = true;

                          // Calculate tx time for all rates of the group
                          WifiModeList vhtMcsList = GetVhtDeviceMcsList ();
                          for (uint8_t i = 0; i < MAX_VHT_GROUP_RATES; i++)
                            {
                              WifiMode mode = vhtMcsList[i];
                              // Check for invalid VHT MCSs and do not add time to array.
                              if (IsValidMcs (GetPhy (), streams, chWidth, mode))
                                {
                                  AddFirstMpduTxTime (groupId, mode, CalculateFirstMpduTxDuration (GetPhy (), streams, sgi, chWidth, mode));
                                  AddMpduTxTime (groupId, mode, CalculateMpduTxDuration (GetPhy (), streams, sgi, chWidth, mode));
                                }
                            }
                          NS_LOG_DEBUG ("Initialized group " << +groupId << ": (" << +streams << "," << +sgi << "," << chWidth << ")");
                        }
                    }
                }
            }
        }
    }
}

void
MinstrelHtWifiManager::SetupMac (const Ptr<WifiMac> mac)
{
  NS_LOG_FUNCTION (this << mac);
  m_legacyManager->SetupMac (mac);
  WifiRemoteStationManager::SetupMac (mac);
}

bool
MinstrelHtWifiManager::IsValidMcs (Ptr<WifiPhy> phy, uint8_t streams, uint16_t chWidth, WifiMode mode)
{
  NS_LOG_FUNCTION (this << phy << +streams << chWidth << mode);
  WifiTxVector txvector;
  txvector.SetNss (streams);
  txvector.SetChannelWidth (chWidth);
  txvector.SetMode (mode);
  return txvector.IsValid ();
}

Time
MinstrelHtWifiManager::CalculateFirstMpduTxDuration (Ptr<WifiPhy> phy, uint8_t streams, uint8_t sgi, uint16_t chWidth, WifiMode mode)
{
  NS_LOG_FUNCTION (this << phy << +streams << +sgi << chWidth << mode);
  WifiTxVector txvector;
  txvector.SetNss (streams);
  txvector.SetGuardInterval (sgi ? 400 : 800);
  txvector.SetChannelWidth (chWidth);
  txvector.SetNess (0);
  txvector.SetStbc (phy->GetStbc ());
  txvector.SetMode (mode);
  txvector.SetPreambleType (WIFI_PREAMBLE_HT_MF);
  NS_LOG_DEBUG("Frame Length="<<m_frameLength<<"\n");
  return phy->CalculateTxDuration (m_frameLength, txvector, phy->GetFrequency (), MPDU_IN_AGGREGATE, 0);
}

Time
MinstrelHtWifiManager::CalculateMpduTxDuration (Ptr<WifiPhy> phy, uint8_t streams, uint8_t sgi, uint16_t chWidth, WifiMode mode)
{
  NS_LOG_FUNCTION (this << phy << +streams << +sgi << chWidth << mode);
  WifiTxVector txvector;
  txvector.SetNss (streams);
  txvector.SetGuardInterval (sgi ? 400 : 800);
  txvector.SetChannelWidth (chWidth);
  txvector.SetNess (0);
  txvector.SetStbc (phy->GetStbc ());
  txvector.SetMode (mode);
  txvector.SetPreambleType (WIFI_PREAMBLE_NONE);
  return phy->CalculateTxDuration (m_frameLength, txvector, phy->GetFrequency (), MPDU_IN_AGGREGATE, 0);
}

Time
MinstrelHtWifiManager::GetFirstMpduTxTime (uint8_t groupId, WifiMode mode) const
{
  NS_LOG_FUNCTION (this << +groupId << mode);
  auto it = m_minstrelGroups[groupId].ratesFirstMpduTxTimeTable.find (mode);
  NS_ASSERT (it != m_minstrelGroups[groupId].ratesFirstMpduTxTimeTable.end ());
  return it->second;
}

void
MinstrelHtWifiManager::AddFirstMpduTxTime (uint8_t groupId, WifiMode mode, Time t)
{
  NS_LOG_FUNCTION (this << +groupId << mode << t);
  m_minstrelGroups[groupId].ratesFirstMpduTxTimeTable.insert (std::make_pair (mode, t));
}

Time
MinstrelHtWifiManager::GetMpduTxTime (uint8_t groupId, WifiMode mode) const
{
  NS_LOG_FUNCTION (this << +groupId << mode);
  auto it = m_minstrelGroups[groupId].ratesTxTimeTable.find (mode);
  NS_ASSERT (it != m_minstrelGroups[groupId].ratesTxTimeTable.end ());
  return it->second;
}

void
MinstrelHtWifiManager::AddMpduTxTime (uint8_t groupId, WifiMode mode, Time t)
{
  NS_LOG_FUNCTION (this << +groupId << mode << t);
  m_minstrelGroups[groupId].ratesTxTimeTable.insert (std::make_pair (mode, t));
}

WifiRemoteStation *
MinstrelHtWifiManager::DoCreateStation (void) const
{
  NS_LOG_FUNCTION (this);
  MinstrelHtWifiRemoteStation *station = new MinstrelHtWifiRemoteStation ();

  // Initialize variables common to both stations.
  station->m_nextStatsUpdate = Simulator::Now () + m_updateStats;
  station->m_col = 0;
  station->m_index = 0;
  station->m_maxTpRate = 0;
  station->m_maxTpRate2 = 0;
  station->m_maxProbRate = 0;
  station->m_nModes = 0;
  station->m_totalPacketsCount = 0;
  station->m_samplePacketsCount = 0;
  station->m_isSampling = false;
  station->m_sampleRate = 0;
  station->m_sampleDeferred = false;
  station->m_shortRetry = 0;
  station->m_longRetry = 0;
  station->m_txrate = 0;
  station->m_initialized = false;

  // Variables specific to HT station
  station->m_sampleGroup = 0;
  station->m_numSamplesSlow = 0;
  station->m_sampleCount = 16;
  station->m_sampleWait = 0;
  station->m_sampleTries = 4;

  station->m_avgAmpduLen = 1;
  station->m_ampduLen = 0;
  station->m_ampduPacketCount = 0;

  // If the device supports HT
  if (HasHtSupported () || HasVhtSupported ())
    {
      /**
       * Assume the station is HT.
       * When correct information available it will be checked.
       */
      station->m_isHt = true;
      std::cout<<"HT HT HT"<<station->m_isHt<<"\n";
    }
  // Use the variable in the station to indicate that the device do not support HT
  else
    {
      station->m_isHt = false;
      std::cout<<"HT HT not not HT"<<station->m_isHt<<"\n";
    }

  return station;
}

void
MinstrelHtWifiManager::CheckInit (MinstrelHtWifiRemoteStation *station)
{
  NS_LOG_FUNCTION (this << station);
  // Note: we appear to be doing late initialization of the table
  // to make sure that the set of supported rates has been initialized
  // before we perform our own initialization.
  if (!station->m_initialized)
    {
      /**
       *  Check if the station supports HT.
       *  Assume that if the device do not supports HT then
       *  the station will not support HT either.
       *  We save from using another check and variable.
       */
      if (!GetHtSupported (station) && !GetVhtSupported (station))
        {
          NS_LOG_INFO ("Non-HT station " << station->m_state->m_address);
          station->m_isHt = false;
          std::cout<<"in nonHT"<<"\n";
          // We will use non-HT minstrel for this station. Initialize the manager.
          m_legacyManager->SetAttribute ("UpdateStatistics", TimeValue (m_updateStats));
          m_legacyManager->SetAttribute ("LookAroundRate", UintegerValue (m_lookAroundRate));
          m_legacyManager->SetAttribute ("EWMA", UintegerValue (m_ewmaLevel));
          m_legacyManager->SetAttribute ("SampleColumn", UintegerValue (m_nSampleCol));
          m_legacyManager->SetAttribute ("PacketLength", UintegerValue (m_frameLength));
          m_legacyManager->SetAttribute ("PrintStats", BooleanValue (m_printStats));
          m_legacyManager->CheckInit (station);
        }
      else
        {
          NS_LOG_DEBUG ("HT station " << station->m_state->m_address);
          station->m_isHt = true;
          station->m_nModes = GetNMcsSupported (station);
          station->m_minstrelTable = MinstrelRate (station->m_nModes);
          station->m_sampleTable = SampleRate (m_numRates, std::vector<uint8_t> (m_nSampleCol));
          InitSampleTable (station);
          RateInit (station);
          std::ostringstream tmp;
          tmp << "minstrel-ht-stats-" << station->m_state->m_address << ".txt";
          station->m_statsFile.open (tmp.str ().c_str (), std::ios::out);
          station->m_initialized = true;
        }
    }
}

void
MinstrelHtWifiManager::DoReportRxOk (WifiRemoteStation *st, double rxSnr, WifiMode txMode)
{
  NS_LOG_FUNCTION (this << st);
  NS_LOG_DEBUG ("DoReportRxOk m_txrate=" << ((MinstrelHtWifiRemoteStation *)st)->m_txrate<<" for station "<<st->m_state->m_address);
}

void
MinstrelHtWifiManager::DoReportRtsFailed (WifiRemoteStation *st)
{
  NS_LOG_FUNCTION (this << st);
  MinstrelHtWifiRemoteStation *station = (MinstrelHtWifiRemoteStation *)st;
  CheckInit (station);
  if (!station->m_initialized)
    {
      return;
    }
  NS_LOG_DEBUG ("DoReportRtsFailed m_txrate = " << station->m_txrate);
  station->m_shortRetry++;
}

void
MinstrelHtWifiManager::DoReportRtsOk (WifiRemoteStation *st, double ctsSnr, WifiMode ctsMode, double rtsSnr)
{
  NS_LOG_FUNCTION (this << st);
}

void
MinstrelHtWifiManager::DoReportFinalRtsFailed (WifiRemoteStation *st)
{
  NS_LOG_FUNCTION (this << st);
  MinstrelHtWifiRemoteStation *station = (MinstrelHtWifiRemoteStation *)st;
  NS_LOG_DEBUG ("Final RTS failed");
  CheckInit (station);
  if (!station->m_initialized)
    {
      return;
    }
  UpdateRetry (station);
}

void
MinstrelHtWifiManager::DoReportDataFailed (WifiRemoteStation *st)
{
  NS_LOG_FUNCTION (this << st);
  MinstrelHtWifiRemoteStation *station = (MinstrelHtWifiRemoteStation *)st;

  CheckInit (station);
  if (!station->m_initialized)
    {
      return;
    }

  NS_LOG_DEBUG ("DoReportDataFailed " << station << "\t rate " << station->m_txrate << "\tlongRetry \t" << station->m_longRetry);

  if (!station->m_isHt)
    {
      m_legacyManager->UpdateRate (station);
    }
  else
    {
      uint8_t rateId = GetRateId (station->m_txrate);
      uint8_t groupId = GetGroupId (station->m_txrate);
      station->m_groupsTable[groupId].m_ratesTable[rateId].numRateAttempt++; // Increment the attempts counter for the rate used.
      UpdateRate (station);
    }
}

void
MinstrelHtWifiManager::DoReportDataOk (WifiRemoteStation *st, double ackSnr, WifiMode ackMode, double dataSnr)
{
  NS_LOG_FUNCTION (this << st << ackSnr << ackMode << dataSnr);
  MinstrelHtWifiRemoteStation *station = (MinstrelHtWifiRemoteStation *) st;

  CheckInit (station);
  if (!station->m_initialized)
    {
      return;
    }

  NS_LOG_DEBUG ("DoReportDataOk m_txrate = " << station->m_txrate << ", attempt = " << station->m_minstrelTable[station->m_txrate].numRateAttempt << ", success = " << station->m_minstrelTable[station->m_txrate].numRateSuccess << " (before update).");

  if (!station->m_isHt)
    {
      station->m_minstrelTable[station->m_txrate].numRateSuccess++;
      station->m_minstrelTable[station->m_txrate].numRateAttempt++;

      m_legacyManager->UpdatePacketCounters (station);

      NS_LOG_DEBUG ("DoReportDataOk m_txrate = " << station->m_txrate << ", attempt = " << station->m_minstrelTable[station->m_txrate].numRateAttempt << ", success = " << station->m_minstrelTable[station->m_txrate].numRateSuccess << " (after update).");

      UpdateRetry (station);
      m_legacyManager->UpdateStats (station);

      if (station->m_nModes >= 1)
        {
          station->m_txrate = m_legacyManager->FindRate (station);
        }
    }
  else
    {
      uint8_t rateId = GetRateId (station->m_txrate);
      uint8_t groupId = GetGroupId (station->m_txrate);
      station->m_groupsTable[groupId].m_ratesTable[rateId].numRateSuccess++;
      station->m_groupsTable[groupId].m_ratesTable[rateId].numRateAttempt++;

      UpdatePacketCounters (station, 1, 0);

      NS_LOG_DEBUG ("DoReportDataOk m_txrate = " << station->m_txrate << ", attempt = " << station->m_minstrelTable[station->m_txrate].numRateAttempt << ", success = " << station->m_minstrelTable[station->m_txrate].numRateSuccess << " (after update).");

      station->m_isSampling = false;
      station->m_sampleDeferred = false;

      UpdateRetry (station);
      if (Simulator::Now () >=  station->m_nextStatsUpdate)
        {
          UpdateStats (station);
        }

      if (station->m_nModes >= 1)
        {
          station->m_txrate = FindRate (station);
        }
    }

  NS_LOG_DEBUG ("Next rate to use TxRate = " << station->m_txrate  );
}

void
MinstrelHtWifiManager::DoReportFinalDataFailed (WifiRemoteStation *st)
{
  NS_LOG_FUNCTION (this << st);
  MinstrelHtWifiRemoteStation *station = (MinstrelHtWifiRemoteStation *) st;

  CheckInit (station);
  if (!station->m_initialized)
    {
      return;
    }

  NS_LOG_DEBUG ("DoReportFinalDataFailed - TxRate=" << station->m_txrate);

  if (!station->m_isHt)
    {
      m_legacyManager->UpdatePacketCounters (station);

      UpdateRetry (station);

      m_legacyManager->UpdateStats (station);
      if (station->m_nModes >= 1)
        {
          station->m_txrate = m_legacyManager->FindRate (station);
        }
    }
  else
    {
      UpdatePacketCounters (station, 0, 1);

      station->m_isSampling = false;
      station->m_sampleDeferred = false;

      UpdateRetry (station);
      if (Simulator::Now () >=  station->m_nextStatsUpdate)
        {
          UpdateStats (station);
        }

      if (station->m_nModes >= 1)
        {
          station->m_txrate = FindRate (station);
        }
    }
  NS_LOG_DEBUG ("Next rate to use TxRate = " << station->m_txrate);
}

void
MinstrelHtWifiManager::DoReportAmpduTxStatus (WifiRemoteStation *st, uint8_t nSuccessfulMpdus, uint8_t nFailedMpdus, double rxSnr, double dataSnr)
{
  NS_LOG_FUNCTION (this << st << +nSuccessfulMpdus << +nFailedMpdus << rxSnr << dataSnr);
  MinstrelHtWifiRemoteStation *station = (MinstrelHtWifiRemoteStation *) st;

  CheckInit (station);
  if (!station->m_initialized)
    {
      return;
    }

  NS_ASSERT_MSG (station->m_isHt, "A-MPDU Tx Status called but no HT or VHT supported.");

  NS_LOG_DEBUG ("DoReportAmpduTxStatus. TxRate=" << station->m_txrate << " SuccMpdus= " <<
                +nSuccessfulMpdus << " FailedMpdus= " << +nFailedMpdus);

  station->m_ampduPacketCount++;
  station->m_ampduLen += nSuccessfulMpdus + nFailedMpdus;

  UpdatePacketCounters (station, nSuccessfulMpdus, nFailedMpdus);

  uint8_t rateId = GetRateId (station->m_txrate);
  uint8_t groupId = GetGroupId (station->m_txrate);
  station->m_groupsTable[groupId].m_ratesTable[rateId].numRateSuccess += nSuccessfulMpdus;
  station->m_groupsTable[groupId].m_ratesTable[rateId].numRateAttempt += nSuccessfulMpdus + nFailedMpdus;

  if (nSuccessfulMpdus == 0 && station->m_longRetry < CountRetries (station))
    {
      // We do not receive a BlockAck. The entire AMPDU fail.
      UpdateRate (station);
    }
  else
    {
      station->m_isSampling = false;
      station->m_sampleDeferred = false;

      UpdateRetry (station);
      if (Simulator::Now () >=  station->m_nextStatsUpdate)
        {
          UpdateStats (station);
        }

      if (station->m_nModes >= 1)
        {
          station->m_txrate = FindRate (station);
        }
      NS_LOG_DEBUG ("Next rate to use TxRate = " << station->m_txrate);
    }
}

void
MinstrelHtWifiManager::UpdateRate (MinstrelHtWifiRemoteStation *station)
{
  NS_LOG_FUNCTION (this << station);

  /**
   * Retry Chain table is implemented here.
   *
   * FIXME
   * Currently, NS3 does not retransmit an entire A-MPDU when BACK is missing
   * but retransmits each MPDU until MPDUs lifetime expires (or a BACK is received).
   * Then, there is no way to control A-MPDU retries (no call to NeedDataRetransmission).
   * So, it is possible that the A-MPDU keeps retrying after longRetry reaches its limit.
   *
   *
   * Try |     LOOKAROUND RATE     | NORMAL RATE
   * -------------------------------------------------------
   *  1  |  Random rate            | Best throughput
   *  2  |  Next best throughput   | Next best throughput
   *  3  |  Best probability       | Best probability
   *
   * Note: For clarity, multiple blocks of if's and else's are used
   * Following implementation in Linux, in MinstrelHT Lowest baserate is not used.
   * Explanation can be found here: http://marc.info/?l=linux-wireless&m=144602778611966&w=2
   */

  CheckInit (station);
  if (!station->m_initialized)
    {
      return;
    }
  station->m_longRetry++;

  /**
   * Get the ids for all rates.
   */
  uint8_t maxTpRateId = GetRateId (station->m_maxTpRate);
  uint8_t maxTpGroupId = GetGroupId (station->m_maxTpRate);
  uint8_t maxTp2RateId = GetRateId (station->m_maxTpRate2);
  uint8_t maxTp2GroupId = GetGroupId (station->m_maxTpRate2);
  uint8_t maxProbRateId = GetRateId (station->m_maxProbRate);
  uint8_t maxProbGroupId = GetGroupId (station->m_maxProbRate);

  /// For normal rate, we're not currently sampling random rates.
  if (!station->m_isSampling)
    {
      /// Use best throughput rate.
      if (station->m_longRetry <  station->m_groupsTable[maxTpGroupId].m_ratesTable[maxTpRateId].retryCount)
        {
          NS_LOG_DEBUG ("Not Sampling; use the same rate again");
          station->m_txrate = station->m_maxTpRate;  //!<  There are still a few retries.
        }

      /// Use second best throughput rate.
      else if (station->m_longRetry < ( station->m_groupsTable[maxTpGroupId].m_ratesTable[maxTpRateId].retryCount +
                                        station->m_groupsTable[maxTp2GroupId].m_ratesTable[maxTp2RateId].retryCount))
        {
          NS_LOG_DEBUG ("Not Sampling; use the Max TP2");
          station->m_txrate = station->m_maxTpRate2;
        }

      /// Use best probability rate.
      else if (station->m_longRetry <= ( station->m_groupsTable[maxTpGroupId].m_ratesTable[maxTpRateId].retryCount +
                                         station->m_groupsTable[maxTp2GroupId].m_ratesTable[maxTp2RateId].retryCount +
                                         station->m_groupsTable[maxProbGroupId].m_ratesTable[maxProbRateId].retryCount))
        {
          NS_LOG_DEBUG ("Not Sampling; use Max Prob");
          station->m_txrate = station->m_maxProbRate;
        }
      else
        {
          NS_FATAL_ERROR ("Max retries reached and m_longRetry not cleared properly. longRetry= " << station->m_longRetry);
        }
    }

  /// We're currently sampling random rates.
  else
    {
      /// Sample rate is used only once
      /// Use the best rate.
      if (station->m_longRetry < 1 + station->m_groupsTable[maxTpGroupId].m_ratesTable[maxTp2RateId].retryCount)
        {
          NS_LOG_DEBUG ("Sampling use the MaxTP rate");
          station->m_txrate = station->m_maxTpRate2;
        }

      /// Use the best probability rate.
      else if (station->m_longRetry <= 1 + station->m_groupsTable[maxTpGroupId].m_ratesTable[maxTp2RateId].retryCount +
               station->m_groupsTable[maxProbGroupId].m_ratesTable[maxProbRateId].retryCount)
        {
          NS_LOG_DEBUG ("Sampling use the MaxProb rate");
          station->m_txrate = station->m_maxProbRate;
        }
      else
        {
          NS_FATAL_ERROR ("Max retries reached and m_longRetry not cleared properly. longRetry= " << station->m_longRetry);
        }
    }
  NS_LOG_DEBUG ("Next rate to use TxRate = " << station->m_txrate);
}

void
MinstrelHtWifiManager::UpdateRetry (MinstrelHtWifiRemoteStation *station)
{
  NS_LOG_FUNCTION (this << station);
  station->m_shortRetry = 0;
  station->m_longRetry = 0;
}

void
MinstrelHtWifiManager::UpdatePacketCounters (MinstrelHtWifiRemoteStation *station, uint8_t nSuccessfulMpdus, uint8_t nFailedMpdus)
{
  NS_LOG_FUNCTION (this << station << +nSuccessfulMpdus << +nFailedMpdus);

  station->m_totalPacketsCount += nSuccessfulMpdus + nFailedMpdus;
  NS_LOG_DEBUG("number of successful mpdus "<<unsigned(nSuccessfulMpdus)<<" failedmpdus are "<<unsigned(nFailedMpdus)<<"\n");
  if (station->m_isSampling)
    {
      station->m_samplePacketsCount += nSuccessfulMpdus + nFailedMpdus;
    }
  if (station->m_totalPacketsCount == ~0)
    {
      station->m_samplePacketsCount = 0;
      station->m_totalPacketsCount = 0;
    }

  if (!station->m_sampleWait && !station->m_sampleTries && station->m_sampleCount > 0)
    {
      station->m_sampleWait = 16 + 2 * station->m_avgAmpduLen;
      station->m_sampleTries = 1;
      station->m_sampleCount--;
    }
}

WifiTxVector
MinstrelHtWifiManager::DoGetDataTxVector (WifiRemoteStation *st)
{
  NS_LOG_FUNCTION (this << st);
  MinstrelHtWifiRemoteStation *station = (MinstrelHtWifiRemoteStation *) st;

  if (!station->m_initialized)
    {
      CheckInit (station);
    }

  if (!station->m_isHt)
    {
      WifiTxVector vector = m_legacyManager->GetDataTxVector (station);
      uint64_t dataRate = vector.GetMode ().GetDataRate (vector);
      if (m_currentRate != dataRate && !station->m_isSampling)
        {
          NS_LOG_DEBUG ("New datarate: " << dataRate);
          m_currentRate = dataRate;
        }
      return vector;
    }
  else
    {
      NS_LOG_DEBUG ("DoGetDataMode m_txrate= " << station->m_txrate);

      uint8_t rateId = GetRateId (station->m_txrate);
      uint8_t groupId = GetGroupId (station->m_txrate);
      uint8_t mcsIndex = station->m_groupsTable[groupId].m_ratesTable[rateId].mcsIndex;

      NS_LOG_DEBUG ("DoGetDataMode rateId= " << +rateId << " groupId= " << +groupId << " mode= " << GetMcsSupported (station, mcsIndex));

      McsGroup group = m_minstrelGroups[groupId];

      // Check consistency of rate selected.
      if ((group.sgi && !GetShortGuardInterval (station)) || group.chWidth > GetChannelWidth (station)  ||  group.streams > GetNumberOfSupportedStreams (station))
        {
          NS_FATAL_ERROR ("Inconsistent group selected. Group: (" << +group.streams <<
                         "," << +group.sgi << "," << group.chWidth << ")" <<
                         " Station capabilities: (" << GetNumberOfSupportedStreams (station) <<
                         "," << GetShortGuardInterval (station) << "," << GetChannelWidth (station) << ")");
        }
      WifiMode mode = GetMcsSupported (station, mcsIndex);
      uint64_t dataRate = mode.GetDataRate (group.chWidth, group.sgi ? 400 : 800, group.streams);
      if (m_currentRate != dataRate && !station->m_isSampling)
        {
          NS_LOG_DEBUG ("New datarate: " << dataRate);
          m_currentRate = dataRate;
        }
      return WifiTxVector (mode, GetDefaultTxPowerLevel (), GetPreambleForTransmission (mode, GetAddress (station)), group.sgi ? 400 : 800, GetNumberOfAntennas (), group.streams, GetNess (station), GetChannelWidthForTransmission (mode, group.chWidth), GetAggregation (station) && !station->m_isSampling, false);
    }
}

WifiTxVector
MinstrelHtWifiManager::DoGetRtsTxVector (WifiRemoteStation *st)
{
  NS_LOG_FUNCTION (this << st);
  MinstrelHtWifiRemoteStation *station = (MinstrelHtWifiRemoteStation *) st;

  if (!station->m_initialized)
    {
      CheckInit (station);
    }

  if (!station->m_isHt)
    {
      return m_legacyManager->GetRtsTxVector (station);
    }
  else
    {
      NS_LOG_DEBUG ("DoGetRtsMode m_txrate=" << station->m_txrate);

      /* RTS is sent in a non-HT frame. RTS with HT is not supported yet in NS3.
       * When supported, decision of using HT has to follow rules in Section 9.7.6 from 802.11-2012.
       * From Sec. 9.7.6.5: "A frame other than a BlockAckReq or BlockAck that is carried in a
       * non-HT PPDU shall be transmitted by the STA using a rate no higher than the highest
       * rate in  the BSSBasicRateSet parameter that is less than or equal to the rate or
       * non-HT reference rate (see 9.7.9) of the previously transmitted frame that was
       * directed to the same receiving STA. If no rate in the BSSBasicRateSet parameter meets
       * these conditions, the control frame shall be transmitted at a rate no higher than the
       * highest mandatory rate of the attached PHY that is less than or equal to the rate
       * or non-HT reference rate (see 9.7.9) of the previously transmitted frame that was
       * directed to the same receiving STA."
       */

      // As we are in Minstrel HT, assume the last rate was an HT rate.
      uint8_t rateId = GetRateId (station->m_txrate);
      uint8_t groupId = GetGroupId (station->m_txrate);
      uint8_t mcsIndex = station->m_groupsTable[groupId].m_ratesTable[rateId].mcsIndex;

      WifiMode lastRate = GetMcsSupported (station, mcsIndex);
      uint64_t lastDataRate = lastRate.GetNonHtReferenceRate ();
      uint8_t nBasicRates = GetNBasicModes ();

      WifiMode rtsRate;
      bool rateFound = false;

      for (uint8_t i = 0; i < nBasicRates; i++)
        {
          uint64_t rate = GetBasicMode (i).GetDataRate (20);
          if (rate <= lastDataRate)
            {
              rtsRate = GetBasicMode (i);
              rateFound = true;
            }
        }

      if (!rateFound)
        {
          Ptr<WifiPhy> phy = GetPhy ();
          uint8_t nSupportRates = phy->GetNModes ();
          for (uint8_t i = 0; i < nSupportRates; i++)
            {
              uint64_t rate = phy->GetMode (i).GetDataRate (20);
              if (rate <= lastDataRate)
                {
                  rtsRate = phy->GetMode (i);
                  rateFound = true;
                }
            }
        }

      NS_ASSERT (rateFound);

      return WifiTxVector (rtsRate, GetDefaultTxPowerLevel (), GetPreambleForTransmission (rtsRate, GetAddress (station)),
                           800, 1, 1, 0, GetChannelWidthForTransmission (rtsRate, GetChannelWidth (station)), GetAggregation (station), false);
    }
}

bool
MinstrelHtWifiManager::DoNeedRetransmission (WifiRemoteStation *st, Ptr<const Packet> packet, bool normally)
{
  NS_LOG_FUNCTION (this << st << packet << normally);

  MinstrelHtWifiRemoteStation *station = (MinstrelHtWifiRemoteStation *)st;

  CheckInit (station);
  if (!station->m_initialized)
    {
      return normally;
    }

  uint32_t maxRetries;

  if (!station->m_isHt)
    {
      maxRetries = m_legacyManager->CountRetries (station);
    }
  else
    {
      maxRetries = CountRetries (station);
    }

  if (station->m_longRetry >= maxRetries)
    {
      NS_LOG_DEBUG ("No re-transmission allowed. Retries: " <<  station->m_longRetry << " Max retries: " << maxRetries);
      return false;
    }
  else
    {
      NS_LOG_DEBUG ("Re-transmit. Retries: " <<  station->m_longRetry << " Max retries: " << maxRetries);
      return true;
    }
}

uint32_t
MinstrelHtWifiManager::CountRetries (MinstrelHtWifiRemoteStation *station)
{
  uint8_t maxProbRateId = GetRateId (station->m_maxProbRate);
  uint8_t maxProbGroupId = GetGroupId (station->m_maxProbRate);
  uint8_t maxTpRateId = GetRateId (station->m_maxTpRate);
  uint8_t maxTpGroupId = GetGroupId (station->m_maxTpRate);
  uint8_t maxTp2RateId = GetRateId (station->m_maxTpRate2);
  uint8_t maxTp2GroupId = GetGroupId (station->m_maxTpRate2);

  if (!station->m_isSampling)
    {
      return station->m_groupsTable[maxTpGroupId].m_ratesTable[maxTpRateId].retryCount +
             station->m_groupsTable[maxTp2GroupId].m_ratesTable[maxTp2RateId].retryCount +
             station->m_groupsTable[maxProbGroupId].m_ratesTable[maxProbRateId].retryCount;
    }
  else
    {
      return 1 + station->m_groupsTable[maxTpGroupId].m_ratesTable[maxTp2RateId].retryCount +
             station->m_groupsTable[maxProbGroupId].m_ratesTable[maxProbRateId].retryCount;
    }
}

bool
MinstrelHtWifiManager::IsLowLatency (void) const
{
  return true;
}

uint16_t
MinstrelHtWifiManager::GetNextSample (MinstrelHtWifiRemoteStation *station)
{
  NS_LOG_FUNCTION (this << station);
  uint8_t sampleGroup = station->m_sampleGroup;
  uint8_t index = station->m_groupsTable[sampleGroup].m_index;
  uint8_t col = station->m_groupsTable[sampleGroup].m_col;
  uint8_t sampleIndex = station->m_sampleTable[index][col];
  uint16_t rateIndex = GetIndex (sampleGroup, sampleIndex);
  NS_LOG_DEBUG ("Next Sample is " << rateIndex);
  SetNextSample (station); //Calculate the next sample rate.
  return rateIndex;
}

void
MinstrelHtWifiManager::SetNextSample (MinstrelHtWifiRemoteStation *station)
{
  NS_LOG_FUNCTION (this << station);
  do
    {
      station->m_sampleGroup++;
      station->m_sampleGroup %= m_numGroups;
    }
  while (!station->m_groupsTable[station->m_sampleGroup].m_supported);

  station->m_groupsTable[station->m_sampleGroup].m_index++;

  uint8_t sampleGroup = station->m_sampleGroup;
  uint8_t index = station->m_groupsTable[station->m_sampleGroup].m_index;
  uint8_t col = station->m_groupsTable[sampleGroup].m_col;

  if (index >= m_numRates)
    {
      station->m_groupsTable[station->m_sampleGroup].m_index = 0;
      station->m_groupsTable[station->m_sampleGroup].m_col++;
      if (station->m_groupsTable[station->m_sampleGroup].m_col >= m_nSampleCol)
        {
          station->m_groupsTable[station->m_sampleGroup].m_col = 0;
        }
      index = station->m_groupsTable[station->m_sampleGroup].m_index;
      col = station->m_groupsTable[sampleGroup].m_col;
    }
  NS_LOG_DEBUG ("New sample set: group= " << +sampleGroup << " index= " << +station->m_sampleTable[index][col]);
}

uint16_t
MinstrelHtWifiManager::FindRate (MinstrelHtWifiRemoteStation *station)
{
  NS_LOG_FUNCTION (this << station);
  NS_LOG_DEBUG ("FindRate packet=" << station->m_totalPacketsCount);

  if ((station->m_samplePacketsCount + station->m_totalPacketsCount) == 0)
    {
      return station->m_maxTpRate;
    }

  // If we have waited enough, then sample.
  if (station->m_sampleWait == 0 && station->m_sampleTries != 0)
    {
      //SAMPLING
      NS_LOG_DEBUG ("Obtaining a sampling rate");
      /// Now go through the table and find an index rate.
      uint16_t sampleIdx = GetNextSample (station);
      NS_LOG_DEBUG ("Sampling rate = " << sampleIdx);

      //Evaluate if the sampling rate selected should be used.
      uint8_t sampleGroupId = GetGroupId (sampleIdx);
      uint8_t sampleRateId = GetRateId (sampleIdx);

      // If the rate selected is not supported, then don't sample.
      if (station->m_groupsTable[sampleGroupId].m_supported && station->m_groupsTable[sampleGroupId].m_ratesTable[sampleRateId].supported)
        {
          /**
           * Sampling might add some overhead to the frame.
           * Hence, don't use sampling for the currently used rates.
           *
           * Also do not sample if the probability is already higher than 95%
           * to avoid wasting airtime.
           */
          HtRateInfo sampleRateInfo = station->m_groupsTable[sampleGroupId].m_ratesTable[sampleRateId];

          NS_LOG_DEBUG ("Use sample rate? MaxTpRate= " << station->m_maxTpRate << " CurrentRate= " << station->m_txrate <<
                        " SampleRate= " << sampleIdx << " SampleProb= " << sampleRateInfo.ewmaProb);

          if (sampleIdx != station->m_maxTpRate && sampleIdx != station->m_maxTpRate2
              && sampleIdx != station->m_maxProbRate && sampleRateInfo.ewmaProb <= 95)
            {

              /**
               * Make sure that lower rates get sampled only occasionally,
               * if the link is working perfectly.
               */

              uint8_t maxTpGroupId = GetGroupId (station->m_maxTpRate);
              uint8_t maxTp2GroupId = GetGroupId (station->m_maxTpRate2);
              uint8_t maxTp2RateId = GetRateId (station->m_maxTpRate2);
              uint8_t maxProbGroupId = GetGroupId (station->m_maxProbRate);
              uint8_t maxProbRateId = GetRateId (station->m_maxProbRate);

              uint8_t maxTpStreams = m_minstrelGroups[maxTpGroupId].streams;
              uint8_t sampleStreams = m_minstrelGroups[sampleGroupId].streams;

              Time sampleDuration = sampleRateInfo.perfectTxTime;
              Time maxTp2Duration = station->m_groupsTable[maxTp2GroupId].m_ratesTable[maxTp2RateId].perfectTxTime;
              Time maxProbDuration = station->m_groupsTable[maxProbGroupId].m_ratesTable[maxProbRateId].perfectTxTime;

              NS_LOG_DEBUG ("Use sample rate? SampleDuration= " << sampleDuration << " maxTp2Duration= " << maxTp2Duration <<
                            " maxProbDuration= " << maxProbDuration << " sampleStreams= " << +sampleStreams <<
                            " maxTpStreams= " << +maxTpStreams);
              if (sampleDuration < maxTp2Duration || (sampleStreams < maxTpStreams && sampleDuration < maxProbDuration))
                {
                  /// Set flag that we are currently sampling.
                  station->m_isSampling = true;

                  /// set the rate that we're currently sampling
                  station->m_sampleRate = sampleIdx;

                  NS_LOG_DEBUG ("FindRate " << "sampleRate=" << sampleIdx);
                  station->m_sampleTries--;
                  return sampleIdx;
                }
              else
                {
                  station->m_numSamplesSlow++;
                  if (sampleRateInfo.numSamplesSkipped >= 20 && station->m_numSamplesSlow <= 2)
                    {
                      /// Set flag that we are currently sampling.
                      station->m_isSampling = true;

                      /// set the rate that we're currently sampling
                      station->m_sampleRate = sampleIdx;

                      NS_LOG_DEBUG ("FindRate " << "sampleRate=" << sampleIdx);
                      station->m_sampleTries--;
                      return sampleIdx;
                    }
                }
            }
        }
    }
  if (station->m_sampleWait > 0)
    {
      station->m_sampleWait--;
    }

  ///	Continue using the best rate.

  NS_LOG_DEBUG ("FindRate " << "maxTpRrate=" << station->m_maxTpRate);
  return station->m_maxTpRate;
}
void
MinstrelHtWifiManager::UpdateStats (MinstrelHtWifiRemoteStation *station)
{
  NS_LOG_FUNCTION (this << station);

  station->m_nextStatsUpdate = Simulator::Now () + m_updateStats;

  station->m_numSamplesSlow = 0;
  station->m_sampleCount = 0;

  double tempProb;

  if (station->m_ampduPacketCount > 0)
    {
      uint32_t newLen = station->m_ampduLen / station->m_ampduPacketCount;
      station->m_avgAmpduLen = ( newLen * (100 - m_ewmaLevel) + (station->m_avgAmpduLen * m_ewmaLevel) ) / 100;
      station->m_ampduLen = 0;
      station->m_ampduPacketCount = 0;
    }

  /* Initialize global rate indexes */
  station->m_maxTpRate = GetLowestIndex (station);
  station->m_maxTpRate2 = GetLowestIndex (station);
  station->m_maxProbRate = GetLowestIndex (station);

  /// Update throughput and EWMA for each rate inside each group.
  for (uint8_t j = 0; j < m_numGroups; j++)
    {
      if (station->m_groupsTable[j].m_supported)
        {
          station->m_sampleCount++;
          NS_LOG_DEBUG("Count of sample = "<<station->m_sampleCount<<"\n");

          /* (re)Initialize group rate indexes */
          station->m_groupsTable[j].m_maxTpRate = GetLowestIndex (station, j);
          station->m_groupsTable[j].m_maxTpRate2 = GetLowestIndex (station, j);
          station->m_groupsTable[j].m_maxProbRate = GetLowestIndex (station, j);

          for (uint8_t i = 0; i < m_numRates; i++)
            {
              if (station->m_groupsTable[j].m_ratesTable[i].supported)
                {
                  station->m_groupsTable[j].m_ratesTable[i].retryUpdated = false;

                  NS_LOG_DEBUG (+i << " " << GetMcsSupported (station, station->m_groupsTable[j].m_ratesTable[i].mcsIndex) <<
                                "\t attempt=" << station->m_groupsTable[j].m_ratesTable[i].numRateAttempt <<
                                "\t success=" << station->m_groupsTable[j].m_ratesTable[i].numRateSuccess);

                  /// If we've attempted something.
                  if (station->m_groupsTable[j].m_ratesTable[i].numRateAttempt > 0)
                    {
                      station->m_groupsTable[j].m_ratesTable[i].numSamplesSkipped = 0;
                      /**
                       * Calculate the probability of success.
                       * Assume probability scales from 0 to 100.
                       */
                      tempProb = (100 * station->m_groupsTable[j].m_ratesTable[i].numRateSuccess) / station->m_groupsTable[j].m_ratesTable[i].numRateAttempt;

                      /// Bookkeeping.
                      station->m_groupsTable[j].m_ratesTable[i].prob = tempProb;

                      if (station->m_groupsTable[j].m_ratesTable[i].successHist == 0)
                        {
                          station->m_groupsTable[j].m_ratesTable[i].ewmaProb = tempProb;
                        }
                      else
                        {
                          station->m_groupsTable[j].m_ratesTable[i].ewmsdProb = CalculateEwmsd (station->m_groupsTable[j].m_ratesTable[i].ewmsdProb,
                                                                                                tempProb, station->m_groupsTable[j].m_ratesTable[i].ewmaProb,
                                                                                                m_ewmaLevel);
                          /// EWMA probability
                          tempProb = (tempProb * (100 - m_ewmaLevel) + station->m_groupsTable[j].m_ratesTable[i].ewmaProb * m_ewmaLevel)  / 100;
                          station->m_groupsTable[j].m_ratesTable[i].ewmaProb = tempProb;
                        }

                      station->m_groupsTable[j].m_ratesTable[i].throughput = CalculateThroughput (station, j, i, tempProb);

                      station->m_groupsTable[j].m_ratesTable[i].successHist += station->m_groupsTable[j].m_ratesTable[i].numRateSuccess;
                      station->m_groupsTable[j].m_ratesTable[i].attemptHist += station->m_groupsTable[j].m_ratesTable[i].numRateAttempt;
                    }
                  else
                    {
                      station->m_groupsTable[j].m_ratesTable[i].numSamplesSkipped++;
                    }

                  /// Bookkeeping.
                  station->m_groupsTable[j].m_ratesTable[i].prevNumRateSuccess = station->m_groupsTable[j].m_ratesTable[i].numRateSuccess;
                  station->m_groupsTable[j].m_ratesTable[i].prevNumRateAttempt = station->m_groupsTable[j].m_ratesTable[i].numRateAttempt;
                  station->m_groupsTable[j].m_ratesTable[i].numRateSuccess = 0;
                  station->m_groupsTable[j].m_ratesTable[i].numRateAttempt = 0;


                  if (station->m_groupsTable[j].m_ratesTable[i].throughput != 0)
                    {
                      SetBestStationThRates (station, GetIndex (j, i));
                      SetBestProbabilityRate (station, GetIndex (j, i));
                    }

                }
            }
        }
    }

  //Try to sample all available rates during each interval.
  station->m_sampleCount *= 8;

  //Recalculate retries for the rates selected.
  CalculateRetransmits (station, station->m_maxTpRate);
  CalculateRetransmits (station, station->m_maxTpRate2);
  CalculateRetransmits (station, station->m_maxProbRate);

  NS_LOG_DEBUG ("max tp=" << station->m_maxTpRate << "\nmax tp2=" <<  station->m_maxTpRate2 << "\nmax prob=" << station->m_maxProbRate);
  if (m_printStats)
    {
      PrintTable (station);
    }
}

double
MinstrelHtWifiManager::CalculateThroughput (MinstrelHtWifiRemoteStation *station, uint8_t groupId, uint8_t rateId, double ewmaProb)
{
  /**
  * Calculating throughput.
  * Do not account throughput if success prob is below 10%
  * (as done in minstrel_ht linux implementation).
  */
  if (ewmaProb < 10)
    {
      return 0;
    }
  else
    {
      /**
       * For the throughput calculation, limit the probability value to 90% to
       * account for collision related packet error rate fluctuation.
       */
      Time txTime =  station->m_groupsTable[groupId].m_ratesTable[rateId].perfectTxTime;
      if (ewmaProb > 90)
        {
          NS_LOG_DEBUG("Txtime="<<txTime.GetSeconds ()<<" TP calc: "<< 90 / txTime.GetSeconds ()<<"\n");
          return 90 / txTime.GetSeconds ();
        }
      else
        {
          NS_LOG_DEBUG("Txtime="<<txTime.GetSeconds ()<<"TP calc: "<< ewmaProb / txTime.GetSeconds ()<<" EWMAPROB="<<ewmaProb<<"\n");
          return ewmaProb / txTime.GetSeconds ();
        }
    }
}

void
MinstrelHtWifiManager::SetBestProbabilityRate (MinstrelHtWifiRemoteStation *station, uint16_t index)
{
  GroupInfo *group;
  HtRateInfo rate;
  uint8_t tmpGroupId, tmpRateId;
  double tmpTh, tmpProb;
  uint8_t groupId, rateId;
  double currentTh;
  // maximum group probability (GP) variables
  uint8_t maxGPGroupId, maxGPRateId;
  double maxGPTh;

  groupId = GetGroupId (index);
  rateId = GetRateId (index);
  group = &station->m_groupsTable[groupId];
  rate = group->m_ratesTable[rateId];

  tmpGroupId = GetGroupId (station->m_maxProbRate);
  tmpRateId = GetRateId (station->m_maxProbRate);
  tmpProb = station->m_groupsTable[tmpGroupId].m_ratesTable[tmpRateId].ewmaProb;
  tmpTh =  station->m_groupsTable[tmpGroupId].m_ratesTable[tmpRateId].throughput;

  if (rate.ewmaProb > 75)
    {
      currentTh = station->m_groupsTable[groupId].m_ratesTable[rateId].throughput;
      if (currentTh > tmpTh)
        {
          station->m_maxProbRate = index;
        }

      maxGPGroupId = GetGroupId (group->m_maxProbRate);
      maxGPRateId = GetRateId (group->m_maxProbRate);
      maxGPTh = station->m_groupsTable[maxGPGroupId].m_ratesTable[maxGPRateId].throughput;

      if (currentTh > maxGPTh)
        {
          group->m_maxProbRate = index;
        }
    }
  else
    {
      if (rate.ewmaProb > tmpProb)
        {
          station->m_maxProbRate = index;
        }
      maxGPRateId = GetRateId (group->m_maxProbRate);
      if (rate.ewmaProb > group->m_ratesTable[maxGPRateId].ewmaProb)
        {
          group->m_maxProbRate = index;
        }
    }
}

/*
 * Find & sort topmost throughput rates
 *
 * If multiple rates provide equal throughput the sorting is based on their
 * current success probability. Higher success probability is preferred among
 * MCS groups.
 */
void
MinstrelHtWifiManager::SetBestStationThRates (MinstrelHtWifiRemoteStation *station, uint16_t index)
{
  uint8_t groupId, rateId;
  double th, prob;
  uint8_t maxTpGroupId, maxTpRateId;
  uint8_t maxTp2GroupId, maxTp2RateId;
  double maxTpTh, maxTpProb;
  double maxTp2Th, maxTp2Prob;

  groupId = GetGroupId (index);
  rateId = GetRateId (index);
  prob = station->m_groupsTable[groupId].m_ratesTable[rateId].ewmaProb;
  th = station->m_groupsTable[groupId].m_ratesTable[rateId].throughput;

  maxTpGroupId = GetGroupId (station->m_maxTpRate);
  maxTpRateId = GetRateId (station->m_maxTpRate);
  maxTpProb = station->m_groupsTable[maxTpGroupId].m_ratesTable[maxTpRateId].ewmaProb;
  maxTpTh = station->m_groupsTable[maxTpGroupId].m_ratesTable[maxTpRateId].throughput;

  maxTp2GroupId = GetGroupId (station->m_maxTpRate2);
  maxTp2RateId = GetRateId (station->m_maxTpRate2);
  maxTp2Prob = station->m_groupsTable[maxTp2GroupId].m_ratesTable[maxTp2RateId].ewmaProb;
  maxTp2Th = station->m_groupsTable[maxTp2GroupId].m_ratesTable[maxTp2RateId].throughput;

  if (th > maxTpTh || (th == maxTpTh && prob > maxTpProb))
    {
      station->m_maxTpRate2 = station->m_maxTpRate;
      station->m_maxTpRate = index;
    }
  else if (th > maxTp2Th || (th == maxTp2Th && prob > maxTp2Prob))
    {
      station->m_maxTpRate2 = index;
    }

  //Find best rates per group

  GroupInfo *group = &station->m_groupsTable[groupId];
  maxTpGroupId = GetGroupId (group->m_maxTpRate);
  maxTpRateId = GetRateId (group->m_maxTpRate);
  maxTpProb = group->m_ratesTable[maxTpRateId].ewmaProb;
  maxTpTh = station->m_groupsTable[maxTpGroupId].m_ratesTable[maxTpRateId].throughput;

  maxTp2GroupId = GetGroupId (group->m_maxTpRate2);
  maxTp2RateId = GetRateId (group->m_maxTpRate2);
  maxTp2Prob = group->m_ratesTable[maxTp2RateId].ewmaProb;
  maxTp2Th = station->m_groupsTable[maxTp2GroupId].m_ratesTable[maxTp2RateId].throughput;

  if (th > maxTpTh || (th == maxTpTh && prob > maxTpProb))
    {
      group->m_maxTpRate2 = group->m_maxTpRate;
      group->m_maxTpRate = index;
    }
  else if (th > maxTp2Th || (th == maxTp2Th && prob > maxTp2Prob))
    {
      group->m_maxTpRate2 = index;
    }
}

void
MinstrelHtWifiManager::RateInit (MinstrelHtWifiRemoteStation *station)
{
  NS_LOG_FUNCTION (this << station);

  station->m_groupsTable = McsGroupData (m_numGroups);

  /**
  * Initialize groups supported by the receiver.
  */
  NS_LOG_DEBUG ("Supported groups by station:");
  for (uint8_t groupId = 0; groupId < m_numGroups; groupId++)
    {
      if (m_minstrelGroups[groupId].isSupported)
        {
          station->m_groupsTable[groupId].m_supported = false;
          if (!(!GetVhtSupported (station) && m_minstrelGroups[groupId].isVht)                    ///Is VHT supported by the receiver?
              && (m_minstrelGroups[groupId].isVht || !GetVhtSupported (station) || !m_useVhtOnly) ///If it is an HT MCS, check if VHT only is disabled
              && !(!GetShortGuardInterval (station) && m_minstrelGroups[groupId].sgi)             ///Is SGI supported by the receiver?
              && (GetChannelWidth (station) >= m_minstrelGroups[groupId].chWidth)                 ///Is channel width supported by the receiver?
              && (GetNumberOfSupportedStreams (station) >= m_minstrelGroups[groupId].streams))    ///Are streams supported by the receiver?
            {
              NS_LOG_DEBUG ("Group " << +groupId << ": (" << +m_minstrelGroups[groupId].streams <<
                            "," << +m_minstrelGroups[groupId].sgi << "," << m_minstrelGroups[groupId].chWidth << ")");

              station->m_groupsTable[groupId].m_supported = true;                                ///Group supported.
              station->m_groupsTable[groupId].m_col = 0;
              station->m_groupsTable[groupId].m_index = 0;

              station->m_groupsTable[groupId].m_ratesTable = HtMinstrelRate (m_numRates);        ///Create the rate list for the group.
              for (uint8_t i = 0; i < m_numRates; i++)
                {
                  station->m_groupsTable[groupId].m_ratesTable[i].supported = false;
                }

              // Initialize all modes supported by the remote station that belong to the current group.
              for (uint8_t i = 0; i < station->m_nModes; i++)
                {
                  WifiMode mode = GetMcsSupported (station, i);

                  ///Use the McsValue as the index in the rate table.
                  ///This way, MCSs not supported are not initialized.
                  uint8_t rateId = mode.GetMcsValue ();
                  if (mode.GetModulationClass () == WIFI_MOD_CLASS_HT)
                    {
                      rateId %= MAX_HT_GROUP_RATES;
                    }

                  if ((m_minstrelGroups[groupId].isVht && mode.GetModulationClass () == WIFI_MOD_CLASS_VHT                       ///If it is a VHT MCS only add to a VHT group.
                       && IsValidMcs (GetPhy (), m_minstrelGroups[groupId].streams, m_minstrelGroups[groupId].chWidth, mode))   ///Check validity of the VHT MCS
                      || (!m_minstrelGroups[groupId].isVht &&  mode.GetModulationClass () == WIFI_MOD_CLASS_HT                  ///If it is a HT MCS only add to a HT group.
                          && mode.GetMcsValue () < (m_minstrelGroups[groupId].streams * 8)                                      ///Check if the HT MCS corresponds to groups number of streams.
                          && mode.GetMcsValue () >= ((m_minstrelGroups[groupId].streams - 1) * 8)))
                    {
                      NS_LOG_DEBUG ("Mode " << +i << ": " << mode << " isVht: " << m_minstrelGroups[groupId].isVht);

                      station->m_groupsTable[groupId].m_ratesTable[rateId].supported = true;
                      station->m_groupsTable[groupId].m_ratesTable[rateId].mcsIndex = i;         ///Mapping between rateId and operationalMcsSet
                      station->m_groupsTable[groupId].m_ratesTable[rateId].numRateAttempt = 0;
                      station->m_groupsTable[groupId].m_ratesTable[rateId].numRateSuccess = 0;
                      station->m_groupsTable[groupId].m_ratesTable[rateId].prob = 0;
                      station->m_groupsTable[groupId].m_ratesTable[rateId].ewmaProb = 0;
                      station->m_groupsTable[groupId].m_ratesTable[rateId].prevNumRateAttempt = 0;
                      station->m_groupsTable[groupId].m_ratesTable[rateId].prevNumRateSuccess = 0;
                      station->m_groupsTable[groupId].m_ratesTable[rateId].numSamplesSkipped = 0;
                      station->m_groupsTable[groupId].m_ratesTable[rateId].successHist = 0;
                      station->m_groupsTable[groupId].m_ratesTable[rateId].attemptHist = 0;
                      station->m_groupsTable[groupId].m_ratesTable[rateId].throughput = 0;
                      station->m_groupsTable[groupId].m_ratesTable[rateId].perfectTxTime = GetFirstMpduTxTime (groupId, GetMcsSupported (station, i));
                      station->m_groupsTable[groupId].m_ratesTable[rateId].retryCount = 0;
                      station->m_groupsTable[groupId].m_ratesTable[rateId].adjustedRetryCount = 0;
                      CalculateRetransmits (station, groupId, rateId);
                    }
                }
            }
        }
    }
  SetNextSample (station);                  /// Select the initial sample index.
  UpdateStats (station);                    /// Calculate the initial high throughput rates.
  station->m_txrate = FindRate (station);   /// Select the rate to use.
}

void
MinstrelHtWifiManager::CalculateRetransmits (MinstrelHtWifiRemoteStation *station, uint16_t index)
{
  NS_LOG_FUNCTION (this << station << index);
  uint8_t groupId = GetGroupId (index);
  uint8_t rateId = GetRateId (index);
  if (!station->m_groupsTable[groupId].m_ratesTable[rateId].retryUpdated)
    {
      CalculateRetransmits (station, groupId, rateId);
    }
}

void
MinstrelHtWifiManager::CalculateRetransmits (MinstrelHtWifiRemoteStation *station, uint8_t groupId, uint8_t rateId)
{
  NS_LOG_FUNCTION (this << station << +groupId << +rateId);

  uint32_t cw = 15;                     // Is an approximation.
  uint32_t cwMax = 1023;
  Time cwTime, txTime, dataTxTime;
  Time slotTime = GetMac ()->GetSlot ();
  Time ackTime = GetMac ()->GetBasicBlockAckTimeout ();

  if (station->m_groupsTable[groupId].m_ratesTable[rateId].ewmaProb < 1)
    {
      station->m_groupsTable[groupId].m_ratesTable[rateId].retryCount = 1;
    }
  else
    {
      station->m_groupsTable[groupId].m_ratesTable[rateId].retryCount = 2;
      station->m_groupsTable[groupId].m_ratesTable[rateId].retryUpdated = true;

      dataTxTime = GetFirstMpduTxTime (groupId, GetMcsSupported (station, station->m_groupsTable[groupId].m_ratesTable[rateId].mcsIndex)) +
        GetMpduTxTime (groupId, GetMcsSupported (station, station->m_groupsTable[groupId].m_ratesTable[rateId].mcsIndex)) * (station->m_avgAmpduLen - 1);

      /* Contention time for first 2 tries */
      cwTime = (cw / 2) * slotTime;
      cw = Min ((cw + 1) * 2, cwMax);
      cwTime += (cw / 2) * slotTime;
      cw = Min ((cw + 1) * 2, cwMax);

      /* Total TX time for data and Contention after first 2 tries */
      txTime = cwTime + 2 * (dataTxTime + ackTime);

      /* See how many more tries we can fit inside segment size */
      do
        {
          /* Contention time for this try */
          cwTime = (cw / 2) * slotTime;
          cw = Min ((cw + 1) * 2, cwMax);

          /* Total TX time after this try */
          txTime += cwTime + ackTime + dataTxTime;
        }
      while ((txTime < MilliSeconds (6))
             && (++station->m_groupsTable[groupId].m_ratesTable[rateId].retryCount < 7));
    }
}

double
MinstrelHtWifiManager::CalculateEwmsd (double oldEwmsd, double currentProb, double ewmaProb, double weight)
{
  double diff, incr, tmp;

  /* calculate exponential weighted moving variance */
  diff = currentProb - ewmaProb;
  incr = (100 - weight) * diff / 100;
  tmp = oldEwmsd * oldEwmsd;
  tmp = weight * (tmp + diff * incr) / 100;

  /* return standard deviation */
  return sqrt (tmp);
}

void
MinstrelHtWifiManager::InitSampleTable (MinstrelHtWifiRemoteStation *station)
{
  NS_LOG_FUNCTION (this << station);
  station->m_col = station->m_index = 0;

  //for off-setting to make rates fall between 0 and nModes
  uint8_t numSampleRates = m_numRates;

  uint16_t newIndex;
  for (uint8_t col = 0; col < m_nSampleCol; col++)
    {
      for (uint8_t i = 0; i < numSampleRates; i++ )
        {
          /**
           * The next two lines basically tries to generate a random number
           * between 0 and the number of available rates
           */
          int uv = m_uniformRandomVariable->GetInteger (0, numSampleRates);
          newIndex = (i + uv) % numSampleRates;

          //this loop is used for filling in other uninitialized places
          while (station->m_sampleTable[newIndex][col] != 0)
            {
              newIndex = (newIndex + 1) % m_numRates;
            }
          station->m_sampleTable[newIndex][col] = i;
        }
    }
}

void
MinstrelHtWifiManager::PrintTable (MinstrelHtWifiRemoteStation *station)
{
  station->m_statsFile << "               best   ____________rate__________    ________statistics________    ________________current(last)________________    _________________last_________________    ___________________________sum-of___________________________\n" <<
    " mode guard #  rate  [name   idx airtime  max_tp]  [avg(tp) avg(prob) sd(prob)]  [prob.|retry|suc|suc in Bytes|att|att in Bytes] [prob.|suc|suc in Bytes|att|att in Bytes] [#success |# success in Bytes| #attempts|# attempts in Bytes]\n";
  for (uint8_t i = 0; i < m_numGroups; i++)
    {
      StatsDump (station, i, station->m_statsFile);
    }

  station->m_statsFile << "\nTotal packet count::    ideal " << Max (0, station->m_totalPacketsCount - station->m_samplePacketsCount) <<
    "              lookaround " << station->m_samplePacketsCount << "\n";
  station->m_statsFile << "Average # of aggregated frames per A-MPDU: " << station->m_avgAmpduLen << "\n\n";

  station->m_statsFile.flush ();
}

void
MinstrelHtWifiManager::StatsDump (MinstrelHtWifiRemoteStation *station, uint8_t groupId, std::ofstream &of)
{
  std::map<Mac48Address,uint> ::iterator itc;
  for(itc = addresslist.begin(); itc != addresslist.end(); itc++)
  {
    //mark the addresses that are already in
    if(itc->second == 1)
    {
      //std::cout<<"The MAC:"<<itc->first<<" is done\n";
      checker++;
    }
    else
    {
      //std::cout<<"The MAC:"<<itc->first<<" is not done\n";
      checker--;
    }
  }

/*if(checker >= 10)
{
  globalchanutil = 0;
  //std::cout<<"all done!\n";
}*/
  uint8_t numRates = m_numRates;
  std::map<Mac48Address,uint32_t> noofrxaggpacketsperstation = MsduAggregator::getnoofrxaggpacketsperstation();
  std::map<Mac48Address,uint32_t> basicpayloadsizeperstation = MsduAggregator::getbasicpayloadsizeperstation();
  std::map<Mac48Address,uint16_t> amsdusizeperstation = MsduAggregator::getamsdusizeperstation();
  std::map<Mac48Address,uint16_t> amsdusizeperstationlast = {};
  std::map<Mac48Address,uint32_t> packetsizeperstation = MacLow::getpacketsizeperstation();
  McsGroup group = m_minstrelGroups[groupId];
  Time txTime;
  char giMode;
  double totalnoofbytesattemptedcurrent = 0;
  double totalnoofbytessuccesscurrent = 0;


  std::ofstream filegp, filecu;
  filegp.open("Goodput"+std::to_string(addressstamap[station->m_state->m_address])+"station.csv",std::fstream::app);
  filecu.open("Channel Utilization"+std::to_string(addressstamap[station->m_state->m_address])+"station.csv",std::fstream::app);

  if (group.sgi)
    {
      giMode = 'S';
    }
  else
    {
      giMode = 'L';
    }


  for (uint8_t i = 0; i < numRates; i++)
    {
      //std::cout<<"What is this?"<<unsigned(i)<<"\n";
      if (station->m_groupsTable[groupId].m_supported && station->m_groupsTable[groupId].m_ratesTable[i].supported)
        {
          if (!group.isVht)
            {
              of << "HT" << group.chWidth << "   " << giMode << "GI  " << (int)group.streams << "   ";
            }
          else
            {
              of << "VHT" << group.chWidth << "   " << giMode << "GI  " << (int)group.streams << "   ";
            }

          uint16_t maxTpRate = station->m_maxTpRate;
          uint16_t maxTpRate2 = station->m_maxTpRate2;
          uint16_t maxProbRate = station->m_maxProbRate;

          uint16_t idx = GetIndex (groupId, i);
          if (idx == maxTpRate)
            {
              of << 'A';
            }
          else
            {
              of << ' ';
            }
          if (idx == maxTpRate2)
            {
              of << 'B';
            }
          else
            {
              of << ' ';
            }
          if (idx == maxProbRate)
            {
              of << 'P';
            }
          else
            {
              of << ' ';
            }

          if (!group.isVht)
            {
              of << std::setw (4) << "   MCS" << (group.streams - 1) * 8 + i;
            }
          else
            {
              of << std::setw (7) << "   MCS" << +i << "/" << (int) group.streams;
            }

          of << "  " << std::setw (3) << +idx << "  ";


          //NS_LOG_DEBUG("The basic payload size sent to station "<<station->m_state->m_address<<" is "<<basicpayloadsizeperstation[station->m_state->m_address]<<" bytes\n");
          /* tx_time[rate(i)] in usec */
          txTime = GetFirstMpduTxTime (groupId, GetMcsSupported (station, station->m_groupsTable[groupId].m_ratesTable[i].mcsIndex));
          of << std::setw (6) << txTime.GetMicroSeconds () << "  ";
          of << std::setw (7) << CalculateThroughput (station, groupId, i, 100) / 100 << "   " <<
            std::setw (7) << station->m_groupsTable[groupId].m_ratesTable[i].throughput / 100 << "   " <<
            std::setw (7) << station->m_groupsTable[groupId].m_ratesTable[i].ewmaProb << "  " <<
            std::setw (7) << station->m_groupsTable[groupId].m_ratesTable[i].ewmsdProb << "  " <<
            std::setw (7) << station->m_groupsTable[groupId].m_ratesTable[i].prob << "  " <<
            std::setw (2) << station->m_groupsTable[groupId].m_ratesTable[i].retryCount << "   " <<
            //current success number

            std::setw (3) << station->m_groupsTable[groupId].m_ratesTable[i].prevNumRateSuccess << "  " <<
            //current success bytes
            std::setw (9) << packetsizeperstation[station->m_state->m_address]*double(station->m_groupsTable[groupId].m_ratesTable[i].prevNumRateSuccess) << "  " <<
            //std::setw (9) << (noofrxaggpacketsperstation[station->m_state->m_address])/double(station->m_totalPacketsCount)*basicpayloadsizeperstation[station->m_state->m_address]*double(station->m_groupsTable[groupId].m_ratesTable[i].prevNumRateSuccess) << "  " <<//put the
            //current attempts number
            std::setw (4) << station->m_groupsTable[groupId].m_ratesTable[i].prevNumRateAttempt << "   " <<
            //current attempts bytes
            std::setw (9) << packetsizeperstation[station->m_state->m_address]*double(station->m_groupsTable[groupId].m_ratesTable[i].prevNumRateAttempt) << "  " <<
            //last prob suc/att
            std::setw (7) << float(station->m_groupsTable[groupId].m_ratesTable[i].lasttoprevNumRateSuccess) / float(station->m_groupsTable[groupId].m_ratesTable[i].lasttoprevNumRateAttempt) << "  " <<
            //last success number
            std::setw (3) << station->m_groupsTable[groupId].m_ratesTable[i].lasttoprevNumRateSuccess << "  " <<
            //last success bytes
            std::setw (9) << packetsizeperstation[station->m_state->m_address]*double(station->m_groupsTable[groupId].m_ratesTable[i].lasttoprevNumRateSuccess) << "  " <<//put the
            //last attempts number
            std::setw (4) << station->m_groupsTable[groupId].m_ratesTable[i].lasttoprevNumRateAttempt << "   " <<
            //last attempts bytes
            std::setw (9) << packetsizeperstation[station->m_state->m_address]*double(station->m_groupsTable[groupId].m_ratesTable[i].lasttoprevNumRateAttempt) << "  " <<
            //total attempts number
            std::setw (9) << station->m_groupsTable[groupId].m_ratesTable[i].successHist << "   " <<
            //total success bytes
            std::setw (15) << packetsizeperstation[station->m_state->m_address]*double(station->m_groupsTable[groupId].m_ratesTable[i].successHist) << "  " <<
            //total attempts number
            std::setw (9) << station->m_groupsTable[groupId].m_ratesTable[i].attemptHist << "  " <<
            //total attempts bytes
            std::setw (15) << packetsizeperstation[station->m_state->m_address]*double(station->m_groupsTable[groupId].m_ratesTable[i].attemptHist) << "\n";
            //add the bytes attempted
          /*  if (idx == maxTpRate && station->m_state->m_address != Mac48Address("00:00:00:00:00:0b") && station->m_state->m_address != Mac48Address("00:00:00:00:00:0c") && station->m_state->m_address != Mac48Address("00:00:00:00:00:0d"))
            {
              totalnoofbytesattemptedcurrent += packetsizeperstation[station->m_state->m_address]*double(station->m_groupsTable[groupId].m_ratesTable[i].prevNumRateAttempt);
              totalnoofbytessuccesscurrent += packetsizeperstation[station->m_state->m_address]*double(station->m_groupsTable[groupId].m_ratesTable[i].prevNumRateSuccess);
            }*/
            //NS_LOG_DEBUG("Time into simulation "<<Simulator::Now ().GetSeconds());
            //channel util= Tdifs(34us)+TB((CWmin(15us)*Tslot(9us))/2)+Tdata(what we have for ns3 success and platform attempts)+Tsifs(16us)+Tack(Tpre(16us)+Tphy(4us)+Tack(time for 14 bytes))

            //put common historical data and previous historical data for both ns3 and platform
            historical_bytes[station->m_state->m_address] = packetsizeperstation[station->m_state->m_address]*double(station->m_groupsTable[groupId].m_ratesTable[i].successHist);


            //for(uint j = 0; j < sizeof(addresslist); j++)
            //{
            uint prevmcs = lastmcsperaddress[station->m_state->m_address];

            if(idx == prevmcs  && station->m_state->m_address != Mac48Address("00:00:00:00:00:0b") && station->m_state->m_address != Mac48Address("00:00:00:00:00:0c") && station->m_state->m_address != Mac48Address("00:00:00:00:00:0d"))//
            {
              if (ns3val)
              {
                //std::cout<<"The last mcs is "<<prevmcs<< " and current mcs is "<<idx<< " MAXTPRATE is "<<maxTpRate<<"\n";
                //for ns3 validation
                std::cout<< packetsizeperstation[station->m_state->m_address]*double(station->m_groupsTable[groupId].m_ratesTable[prevmcs].prevNumRateSuccess)<<" is the success bytes at prev mcs "<<unsigned(prevmcs)<<"\n";
                std::cout<<"MAC add:"<<station->m_state->m_address<<" for attempts "<<double(station->m_groupsTable[groupId].m_ratesTable[prevmcs].prevNumRateSuccess)<<"at MCS "<< unsigned(prevmcs)<< "\n";
                totalnoofbytessuccesscurrent = packetsizeperstation[station->m_state->m_address]*double(station->m_groupsTable[groupId].m_ratesTable[prevmcs].prevNumRateSuccess);
                //std::cout<<"no of currnet attempt bytes:"<<totalnoofbytessuccesscurrent<<" from MAC "<<station->m_state->m_address<<" at MCS "<<prevmcs<<"\n";
                chanutil[station->m_state->m_address] = totalnoofbytessuccesscurrent*8/(1024*1024*mcsphyrate[prevmcs]*0.1);//channel util for platform 0.1 for minstrel window 100ms
                successrations3[station->m_state->m_address] = float(station->m_groupsTable[groupId].m_ratesTable[prevmcs].lasttoprevNumRateSuccess) / float(station->m_groupsTable[groupId].m_ratesTable[prevmcs].lasttoprevNumRateAttempt)*100;
                //station->m_statsFile << "\nGoodput:"<<float(packetsizeperstation[station->m_state->m_address]*double(station->m_groupsTable[groupId].m_ratesTable[i].lasttoprevNumRateSuccess)*8/(1024*1024*0.1))<<"\n Channel Utilization:"<<chanutil[station->m_state->m_address]<<"\n";
                filegp <<float(packetsizeperstation[station->m_state->m_address]*double(station->m_groupsTable[groupId].m_ratesTable[i].lasttoprevNumRateSuccess)*8/(1024*1024*0.1))<<",";
                filecu << chanutil[station->m_state->m_address]<<",";
              }

              else
              {
                //for platform validation
                totalnoofbytesattemptedcurrent += packetsizeperstation[station->m_state->m_address]*double(station->m_groupsTable[groupId].m_ratesTable[prevmcs].prevNumRateAttempt);// the time taken for the data is?
                std::cout<<"no of currnet attempt bytes:"<<totalnoofbytesattemptedcurrent<<" from MAC "<<station->m_state->m_address<<"\n";
                std::cout<<" num of attempts:"<<station->m_groupsTable[groupId].m_ratesTable[prevmcs].prevNumRateAttempt<<"\n";
                //std::cout<<"last attempts "<<double(station->m_groupsTable[groupId].m_ratesTable[i].prevNumRateAttempt)<<" and the product is"<<double(station->m_groupsTable[groupId].m_ratesTable[i].prevNumRateAttempt)*(0.000034+0.000012+0.000016+0.000016+0.000004+0.000002154)<<"\n";
                float inus = float((station->m_groupsTable[groupId].m_ratesTable[prevmcs].prevNumRateAttempt)*(0.000034+0.000012+0.000016+0.000016+0.000004+0.000002154)/0.1);
                float datas = float(totalnoofbytesattemptedcurrent*8/(1024*1024*mcsphyrate[prevmcs]*0.1));

                std::cout<<inus<< " frill in seconds\n";
                std::cout<<datas<< " data  in seconds\n";
                chanutil[station->m_state->m_address] = datas + inus;//+(double(station->m_groupsTable[groupId].m_ratesTable[i].prevNumRateAttempt)*(0.000034+0.000012+0.000016+0.000016+0.000004+0.000002154));
                successratioplatform[station->m_state->m_address] = float(station->m_groupsTable[groupId].m_ratesTable[prevmcs].lasttoprevNumRateSuccess) / float(station->m_groupsTable[groupId].m_ratesTable[prevmcs].lasttoprevNumRateAttempt);
                last_attempt_bytes[station->m_state->m_address] = packetsizeperstation[station->m_state->m_address]*double(station->m_groupsTable[groupId].m_ratesTable[prevmcs].lasttoprevNumRateSuccess);
                minstrel_throughput[station->m_state->m_address] = station->m_groupsTable[groupId].m_ratesTable[prevmcs].throughput / 100;
                //station->m_statsFile << "\nGoodput:"<<float(packetsizeperstation[station->m_state->m_address]*double(station->m_groupsTable[groupId].m_ratesTable[i].lasttoprevNumRateSuccess)*8/(1024*1024*0.1))<<"\n Channel Utilization:"<<chanutil[station->m_state->m_address];
                filegp <<float(packetsizeperstation[station->m_state->m_address]*double(station->m_groupsTable[groupId].m_ratesTable[i].lasttoprevNumRateSuccess)*8/(1024*1024*0.1))<<",";
                filecu << chanutil[station->m_state->m_address]<<",";
              }
              addresslist[station->m_state->m_address] = 1;

            }





            std::map<Mac48Address,float> ::iterator itb;
            for(itb = chanutil.begin(); itb != chanutil.end(); itb++)
            {
              std::cout << "The station MAC "<<(itb->first)<<" channel utilisation "<<itb->second<<"\n";// << " " << it->second.first << " " <<it->second.second << "\n";
              globalchanutil+=itb->second;
            }

            //global ch util for platform in % as attemptsand multiply ch access for each packet
            //global ch util for ns3 in 0,1 as sucess and no channel accesss


            /*if(globalchanutil>0.5)
            {
              MsduAggregator::changeamsdusizeperstation(station->m_state->m_address, 3839);
            }*/
            if (!ns3val)
            {
              globalchanutilplatform = globalchanutil*100;
              std::cout<<" percent of Channel Utilization in platform:"<<globalchanutilplatform<<"\n";
              //globalchanutil=globalchanutil*100;//in % for ns3 and as it is for platform
              //MsduAggregator::setamsdusizeperstation(station->m_state->m_address, comparelastandcurrentplatform(successratioplatform[station->m_state->m_address], globalchanutilns3, last_attempt_bytes[station->m_state->m_address], minstrel_throughput[station->m_state->m_address], idx, lastmcsperaddress[station->m_state->m_address],station->m_state->m_address));
              MsduAggregator::setamsdusizeperstation(station->m_state->m_address, comparelastandcurrentrfrplatform(successratioplatform[station->m_state->m_address], globalchanutilns3, last_attempt_bytes[station->m_state->m_address], minstrel_throughput[station->m_state->m_address], idx, lastmcsperaddress[station->m_state->m_address],station->m_state->m_address));
            }

            else
            {
              globalchanutilns3 = globalchanutil;
              std::cout<<" Channel Utilization in NS3:"<<globalchanutilns3<<"\n";
              //where the compa
              //MsduAggregator::setamsdusizeperstation(station->m_state->m_address, comparelastandcurrent(successrations3[station->m_state->m_address], globalchanutilns3, idx, lastmcsperaddress[station->m_state->m_address],station->m_state->m_address));
              //MsduAggregator::setamsdusizeperstation(station->m_state->m_address, comparelastandcurrentrfrns3(successrations3[station->m_state->m_address], globalchanutilns3, idx, lastmcsperaddress[station->m_state->m_address],station->m_state->m_address));

            }

            std::cout<<Simulator::Now ()<<" TIME\n";
            //for ns3 M5P model


          //}
            station->m_groupsTable[groupId].m_ratesTable[i].lasttoprevNumRateAttempt = station->m_groupsTable[groupId].m_ratesTable[i].prevNumRateAttempt;
            station->m_groupsTable[groupId].m_ratesTable[i].lasttoprevNumRateSuccess = station->m_groupsTable[groupId].m_ratesTable[i].prevNumRateSuccess;
            previous_historical_bytes[station->m_state->m_address] = historical_bytes[station->m_state->m_address];
            //of<<"\nChannel Utilization = "<<globalchanutil<<"\n";
            //amsdusizeperstationlast = amsdusizeperstation;

        }
        globalchanutil = 0;
    }
    lastmcsperaddress[station->m_state->m_address] = station->m_maxTpRate;

    //std::cout<<"Global Channel Utilization "<<globalchanutil<<"\n";


}
uint16_t
MinstrelHtWifiManager::GetIndex (uint8_t groupId, uint8_t rateId)
{
  NS_LOG_FUNCTION (this << +groupId << +rateId);
  uint16_t index;
  index = groupId * m_numRates + rateId;
  return index;
}

uint8_t
MinstrelHtWifiManager::GetRateId (uint16_t index)
{
  NS_LOG_FUNCTION (this << index);
  uint8_t id;
  id = index % m_numRates;
  return id;
}

uint8_t
MinstrelHtWifiManager::GetGroupId (uint16_t index)
{
  NS_LOG_FUNCTION (this << index);
  return index / m_numRates;
}

uint8_t
MinstrelHtWifiManager::GetHtGroupId (uint8_t txstreams, uint8_t sgi, uint16_t chWidth)
{
  NS_LOG_FUNCTION (this << +txstreams << +sgi << chWidth);
  return MAX_SUPPORTED_STREAMS * 2 * (chWidth == 40 ? 1 : 0) + MAX_SUPPORTED_STREAMS * sgi + txstreams - 1;
}

uint8_t
MinstrelHtWifiManager::GetVhtGroupId (uint8_t txstreams, uint8_t sgi, uint16_t chWidth)
{
  NS_LOG_FUNCTION (this << +txstreams << +sgi << chWidth);
  return MAX_HT_STREAM_GROUPS * MAX_SUPPORTED_STREAMS + MAX_SUPPORTED_STREAMS * 2 * (chWidth == 160 ? 3 : chWidth == 80 ? 2 : chWidth == 40 ? 1 : 0) + MAX_SUPPORTED_STREAMS * sgi + txstreams - 1;
}

uint16_t
MinstrelHtWifiManager::GetLowestIndex (MinstrelHtWifiRemoteStation *station)
{
  NS_LOG_FUNCTION (this << station);

  uint8_t groupId = 0;
  uint8_t rateId = 0;
  while (groupId < m_numGroups && !station->m_groupsTable[groupId].m_supported)
    {
      groupId++;
    }
  while (rateId < m_numRates && !station->m_groupsTable[groupId].m_ratesTable[rateId].supported)
    {
      rateId++;
    }
  NS_ASSERT (station->m_groupsTable[groupId].m_supported && station->m_groupsTable[groupId].m_ratesTable[rateId].supported);
  return GetIndex (groupId, rateId);
}

uint16_t
MinstrelHtWifiManager::GetLowestIndex (MinstrelHtWifiRemoteStation *station, uint8_t groupId)
{
  NS_LOG_FUNCTION (this << station << +groupId);

  uint8_t rateId = 0;
  while (rateId < m_numRates && !station->m_groupsTable[groupId].m_ratesTable[rateId].supported)
    {
      rateId++;
    }
  NS_ASSERT (station->m_groupsTable[groupId].m_supported && station->m_groupsTable[groupId].m_ratesTable[rateId].supported);
  return GetIndex (groupId, rateId);
}

WifiModeList
MinstrelHtWifiManager::GetVhtDeviceMcsList (void) const
{
  WifiModeList vhtMcsList;
  Ptr<WifiPhy> phy = GetPhy ();
  for (uint8_t i = 0; i < phy->GetNMcs (); i++)
    {
      WifiMode mode = phy->GetMcs (i);
      if (mode.GetModulationClass () == WIFI_MOD_CLASS_VHT)
        {
          vhtMcsList.push_back (mode);
        }
    }
  return vhtMcsList;
}

WifiModeList
MinstrelHtWifiManager::GetHtDeviceMcsList (void) const
{
  WifiModeList htMcsList;
  Ptr<WifiPhy> phy = GetPhy ();

  for (uint8_t i = 0; i < phy->GetNMcs (); i++)// phy->GetNMcs ();
    {

      NS_LOG_DEBUG("inside GetHtDeviceMcsList");
      WifiMode mode = phy->GetMcs (i);
      if (mode.GetModulationClass () == WIFI_MOD_CLASS_HT)
        {
          htMcsList.push_back (mode);
        }
    }
  return htMcsList;
}

void
MinstrelHtWifiManager::SetHeSupported (bool enable)
{
  //HE is not supported yet by this algorithm.
  if (enable)
    {
      NS_FATAL_ERROR ("WifiRemoteStationManager selected does not support HE rates");
    }
}

////M5P model for ns3
std::tuple<int, float> MinstrelHtWifiManager::m5pmodelns3 (float succratio, float gcu, uint16_t mcsvalue, Mac48Address stadd)
{
  float expected_th = 0;

  //std::cout<<"ns3";
  std::map<int,float> ::iterator expit;
  //MCS0
  if (mcsvalue == 0)
  {
    std::cout<<"The MCS value is "<<mcsvalue<<"\n";
    for(expit = mcs0agglengthexpth.begin(); expit != mcs0agglengthexpth.end(); expit++)
    {
      if(expit->first == 550)
      {
          expected_th = 0.1065*succratio;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
      }

      if(expit->first == 1024)
      {
        if (succratio > 78.819)
        {
          expected_th = 7.4693 * gcu+ 0.11 * succratio- 4.6582;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (gcu >0.614)
        {
          expected_th = 0.5711 * gcu + 0.1122 * succratio	- 0.3711;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th = 0.1065*succratio;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

      if(expit->first == 2048)
      {
        if (succratio > 91.498 && gcu > 0.672)
        {
          expected_th = 0.8746 * gcu + 0.1072 * succratio - 0.1556;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th = 4.3735 * gcu+ 0.1079 * succratio- 2.8601;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

      if(expit->first == 3839)
      {
        if (succratio > 93.638 && gcu <= 0.795 && gcu>0.698)
        {
          expected_th = 0.841 * gcu	+ 0.0979 * succratio + 0.7236;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (succratio <= 96.612)
        {
          expected_th = 3.6633 * gcu+ 0.1024 * succratio- 1.9955;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th =  -0.1888 *gcu	+ 0.1118 * succratio+ 0.1685;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

    }
    genagglengthexpth = mcs0agglengthexpth;
  }
  //MCS1
  if (mcsvalue == 1)
  {
    std::cout<<"The MCS value is "<<mcsvalue<<"\n";
    for(expit = mcs1agglengthexpth.begin(); expit != mcs1agglengthexpth.end(); expit++)
    {
      if(expit->first == 550)
      {
          expected_th = 0.2044*succratio;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
      }

      if(expit->first == 1024)
      {
        if (succratio > 79.686)
        {
          expected_th = 16.7195 * gcu + 0.2136 * succratio- 9.7194;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th = 0.2143 * succratio- 0.0133;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

      if(expit->first == 2048)
      {
        if (succratio > 90.82 && gcu > 0.673)
        {
          expected_th = 0.8746 * gcu + 0.1072 * succratio - 0.1556;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (succratio > 85.741 && gcu <= 97.17)
        {
          expected_th = 10.8694 * gcu	+ 0.207 * succratio	- 6.7998;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (succratio > 90.46)
        {
          expected_th = 4.8577 * gcu	+ 0.2104 * succratio- 3.2008;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th = 0.2044 * succratio+ 0.0009;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

      if(expit->first == 3839)
      {
        if (succratio > 97.881 && gcu <= 0.798 && gcu>0.776)
        {
          expected_th = 1.6119 * gcu+ 0.01 *succratio+ 18.8831;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (succratio <= 97.785)
        {
          expected_th = 9.7826 * gcu+ 0.207 * succratio	- 6.5827;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (gcu <= 0.785 && gcu>0.752)
        {
          expected_th = 1.3728 * gcu+ 0.1229 * succratio+ 7.7557;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (gcu <= 0.771)
        {
          expected_th = 0.0483 * succratio+ 16.5091;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (gcu > 0.693)
        {
          expected_th = 2.9636 * gcu+ 0.2102 *succratio - 1.7841;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th =  20.0043;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

    }

    genagglengthexpth = mcs1agglengthexpth;

  }
//MCS2
  if (mcsvalue == 2)
  {
    std::cout<<"The MCS value is "<<mcsvalue<<"\n";
    for(expit = mcs2agglengthexpth.begin(); expit != mcs2agglengthexpth.end(); expit++)
    {
      if(expit->first == 550)
      {
          expected_th =  0.2935 * succratio;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
      }

      if(expit->first == 1024)
      {
        if (succratio > 59.744)
        {
          expected_th = 26.3252 * gcu	+ 0.3032 * succratio- 13.5384;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th =0.8266 * gcu	+ 0.3062 * succratio- 0.4331;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

      if(expit->first == 2048)
      {
        if (succratio > 92.165 && succratio <= 97.568 && gcu > 0.59)
        {
          expected_th = 1.0879 * gcu+ 0.2707 *succratio	+ 2.9104;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (succratio > 86.382 && gcu > 0.606)
        {
          expected_th =  3.2049 * gcu	+ 0.0235 * succratio+ 25.7931;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th = 7.462 * gcu + 0.2943 * succratio- 4.2939;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

      if(expit->first == 3839)
      {
        if (succratio <= 98.78 && gcu <= 0.795 && gcu > 0.677)
        {
          expected_th = 10.8869 * gcu	+ 0.2995 * succratio- 7.1359;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th =  12.6973 *gcu	+ 0.2929 * succratio - 8.0281;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

    }
    genagglengthexpth = mcs2agglengthexpth;
  }
//MCS3
  if (mcsvalue == 3)
  {
    std::cout<<"The MCS value is "<<mcsvalue<<"\n";
    for(expit = mcs3agglengthexpth.begin(); expit != mcs3agglengthexpth.end(); expit++)
    {
      if(expit->first == 550)
      {
          expected_th = 0.3805*succratio;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
      }

      if(expit->first == 1024)
      {
        if (succratio > 53.832)
        {
          expected_th =  0.3923 * succratio- 0.7937;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th = -0.011 * gcu+ 0.3879 * succratio- 0.0008;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

      if(expit->first == 2048)
      {

          expected_th = 0.3861 * succratio+ 0.0068;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;

      }

      if(expit->first == 3839)
      {
        if (succratio > 98.993)
        {
          expected_th = 0.0497 * succratio+ 33.6317;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (succratio <= 81.211)
        {
          expected_th =0.3888 * succratio	- 0.0021;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th =  0.3339 * succratio+ 4.6991;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

    }
    genagglengthexpth = mcs3agglengthexpth;
  }
//MCS4
  if (mcsvalue == 4)
  {
    std::cout<<"The MCS value is "<<mcsvalue<<"\n";
    for(expit = mcs4agglengthexpth.begin(); expit != mcs4agglengthexpth.end(); expit++)
    {
      if(expit->first == 550)
      {
          expected_th = 0.5327*succratio;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
      }

      if(expit->first == 1024)
      {

          expected_th = 0.5375 * succratio- 0.0447;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;

      }

      if(expit->first == 2048)
      {

          expected_th = 2.1708 * gcu	+ 0.5375 * succratio- 1.0886;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;

      }

      if(expit->first == 3839)
      {
        if (succratio >99.552)
        {
          expected_th = 20.7865 * gcu+ 0.504 * succratio- 9.3981;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (gcu <= 0.654)
        {
          expected_th = 1.0183 * gcu+ 0.4926 *succratio	+ 3.9707;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th =  0.5409 * succratio	- 0.012;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

    }
    genagglengthexpth = mcs4agglengthexpth;
  }
  //MCS5
  if (mcsvalue == 5)
  {
    std::cout<<"The MCS value is "<<mcsvalue<<"\n";
    for(expit = mcs5agglengthexpth.begin(); expit != mcs5agglengthexpth.end(); expit++)
    {
      if(expit->first == 550)
      {
          expected_th = 0.6632*succratio;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
      }

      if(expit->first == 1024)
      {

          expected_th = 44.6962 * gcu	+ 0.6693 * succratio- 15.0703;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;

      }

      if(expit->first == 2048)
      {
        if (succratio > 96.875 && gcu > 0.499)
        {
          expected_th = 2.5334 * gcu	+ 0.0791 * succratio+ 57.4858;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (succratio > 51.569 && succratio <= 97.661 && gcu <= 0.499)
        {
          expected_th = -10.7856 * gcu+ 0.1185 *succratio	+ 56.4639;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (succratio < 51.569)
        {
          expected_th = 0.6831 * succratio- 0.0727;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th = 0.6947 * succratio;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

      if(expit->first == 3839)
      {
        if (succratio > 99.708 && gcu <= 0.626)
        {
          expected_th = 6.9998 * gcu	+ 0.0932 * succratio	+ 53.8532;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (gcu > 0.626 && gcu <= 0.635)
        {
          expected_th = -10.4231 * gcu+ 0.0416 * succratio+ 70.2946;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (gcu > 0.628)
        {
          expected_th = 4.761 * gcu	+ 0.0808 * succratio+ 56.6005;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (succratio <= 98.705 && gcu <= 0.585)
        {
          expected_th = 7.4106 * gcu+ 0.4835 * succratio+ 13.544;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (succratio <= 96.831)
        {
          expected_th = 12.9709 * gcu	+ 0.1891 * succratio+ 39.7829;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (gcu <= 0.585)
        {
          expected_th = 12.0964 * gcu+ 59.2605;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th =   0.6949 * succratio- 0.0143;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

    }
    genagglengthexpth = mcs5agglengthexpth;
  }
  //MCS6
  if (mcsvalue == 6)
  {
    std::cout<<"The MCS value is "<<mcsvalue<<"\n";
    for(expit = mcs6agglengthexpth.begin(); expit != mcs6agglengthexpth.end(); expit++)
    {
      if(expit->first == 550)
      {
          expected_th = 0.7262*succratio;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
      }

      if(expit->first == 1024)
      {
          expected_th = -53.9376 * gcu+ 0.7194 * succratio+ 16.9493;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;

      }

      if(expit->first == 2048)
      {
        if (succratio > 51.788 && succratio <= 96.657)
        {
          expected_th = -0.2507 * gcu	+ 0.6994 *succratio	+ 2.3779;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (succratio > 61.563)
        {
          expected_th = -0.5177 * gcu	+ 0.0663 * succratio+ 65.3986;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th = -1.6826 * gcu	+ 0.7299 * succratio+ 0.662;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

      if(expit->first == 3839)
      {
        if (gcu <= 0.583)
        {
          expected_th = -24.731 * gcu+ 0.6963 * succratio	+ 15.9811;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (gcu <= 0.603)
        {
          expected_th = 6.418 * gcu+ 0.6908 * succratio	- 0.0373;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (gcu <= 0.612)
        {
          expected_th = 10.2135 * gcu	+ 0.7072 * succratio- 3.4352;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th =  -0.0961 * gcu+ 0.7391 * succratio+ 0.0014;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

    }
    genagglengthexpth = mcs6agglengthexpth;
  }
  //MCS7
  if (mcsvalue == 7)
  {
    std::cout<<"The MCS value is "<<mcsvalue<<"\n";
    for(expit = mcs7agglengthexpth.begin(); expit != mcs7agglengthexpth.end(); expit++)
    {
      if(expit->first == 550)
      {
          expected_th = 0.7719*succratio;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
      }

      if(expit->first == 1024)
      {
          expected_th =98.8624 * gcu+ 0.7832 * succratio- 29.3012;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;

      }

      if(expit->first == 2048)
      {
        if (succratio > 49.435)
        {
          expected_th = 10.1735 * gcu+ 0.7847 * succratio- 4.7744;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th = 0.7122 * gcu+ 0.7716 * succratio- 0.2459;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

      if(expit->first == 3839)
      {
        if (succratio > 99.883 && gcu>0.576)
        {
          expected_th = 40.1983 * gcu	+ 0.344 * succratio	+ 19.7195;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (gcu <= 0.544)
        {
          expected_th = 2.2553 * gcu+ 1.1655 * succratio- 40.4109;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (gcu <= 0.571)
        {
          expected_th = -40.6888 * gcu+ 102.3039;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th =   0.7803 *succratio	+ 0.0278;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

    }
    genagglengthexpth = mcs7agglengthexpth;
  }

  //find the max exp_th from the 4 scenarios
  uint last_counter=0;
  float highestexp = 0;
  int highexpagglength = 0;
  std::map<int, float>::iterator genit;
  for(genit = genagglengthexpth.begin(); genit != genagglengthexpth.end(); genit++)
  {
    if (highestexp < genit->second)
    {
      highestexp = genit->second;
      highexpagglength = genit->first;
    }

  }

  // previousaggregationlengthperstation is used
  previousaggregationlengthperstation = MsduAggregator::getamsdusizeperstation();
  if (!previous_statistics[stadd])
  {
      previous_statistics[stadd] = (historical_bytes[stadd]-previous_historical_bytes[stadd])/(1024*1024);
  }
  else
  {
    second_statistics[stadd] = previous_statistics[stadd];
    previous_statistics[stadd] = (historical_bytes[stadd]-previous_historical_bytes[stadd])/(1024*1024);

    if (previousaggregationlengthperstation[stadd] != highexpagglength)
    {
      last_counter = 0;
    }
    else
    {
      last_counter+=1;
    }
    if ((second_statistics[stadd] - previous_statistics[stadd])/second_statistics[stadd] < -0.15 && previousaggregationlengthperstation[stadd] != highexpagglength)
    {

      if(bannedagglengthcounterperstation[stadd][agglengthtoindexmap[previousaggregationlengthperstation[stadd]]] == 0)
      {
        std::cout<<"\n";
        bannedagglengthcounterperstation[stadd][agglengthtoindexmap[previousaggregationlengthperstation[stadd]]] = 10;

      }
      else
      {
        bannedagglengthcounterperstation[stadd][agglengthtoindexmap[previousaggregationlengthperstation[stadd]]] += 10;
      }
        //.. check if banned has the length
      //else{}
    }

    if((previous_statistics[stadd] - second_statistics[stadd])/previous_statistics[stadd] < -0.15 && previousaggregationlengthperstation[stadd] != highexpagglength)
    {
      if(bannedagglengthcounterperstation[stadd][agglengthtoindexmap[highexpagglength]] == 0)
      {
        std::cout<<"\n";
        bannedagglengthcounterperstation[stadd][agglengthtoindexmap[highexpagglength]] = 10;

      }
      else
      {
        bannedagglengthcounterperstation[stadd][agglengthtoindexmap[highexpagglength]] += 10;
      }
    }
  }


  uint sm_counter = 0;
  std::map<int, float>::iterator afterbanit;
  for(afterbanit = genagglengthexpth.begin(); afterbanit != genagglengthexpth.end(); afterbanit++)
  {
    sm_counter = bannedagglengthcounterperstation[stadd][agglengthtoindexmap[afterbanit->first]];
    if (sm_counter != 0)
    {
      afterbanit->second = (afterbanit->second)-((afterbanit->second)*sm_counter/100);
    }

    if(bannedagglengthcounterperstation[stadd][agglengthtoindexmap[highexpagglength]] > 0 || last_counter >=5)
    {
      bannedagglengthcounterperstation[stadd][agglengthtoindexmap[highexpagglength]]--;

    }
    else
    {
      if (highestexp < afterbanit->second)
      {
        highestexp = afterbanit->second;
        highexpagglength = afterbanit->first;
        if (previousaggregationlengthperstation[stadd] != highexpagglength)
        {
          last_counter = 0;
        }
      }
    }
  }

  std::cout<<"The expected throughput is "<<highestexp<<" for aggregation length:"<<highexpagglength<<"\n";
  return std::make_tuple(highexpagglength,highestexp);
}


//compare last and current mcs m5p ns3
int MinstrelHtWifiManager::comparelastandcurrent (float succratio, float gcu, uint16_t currmcs, uint16_t lastmcs, Mac48Address stadd)
{
  float difference;
  float agglength = 0;

  //only the exp_th values
  difference = (std::get<1>(m5pmodelns3(succratio, gcu, currmcs, stadd)) - std::get<1>(m5pmodelns3(succratio, gcu, lastmcs, stadd)))/std::get<1>(m5pmodelns3(succratio, gcu, lastmcs, stadd));
  std::cout<<"At the station MAC:"<<stadd<<" the current expected throughput is "<<std::get<1>(m5pmodelns3(succratio, gcu, currmcs, stadd))<<" the last expected throughput is "<<std::get<1>(m5pmodelns3(succratio, gcu, lastmcs, stadd))<< "\n";
  if (difference < 0.05)
  {
    //derive agg length
    //std::cout<<"ad";
    agglength = std::get<0>(m5pmodelns3(succratio, gcu, lastmcs, stadd));
  }
  else
  {
    //std::cout<<"ad";
    agglength = std::get<0>(m5pmodelns3(succratio, gcu, currmcs, stadd));
  }


  return agglength;
}




////RFR model for ns3
std::tuple<int, float> MinstrelHtWifiManager::rfrmodelns3 (float succratio, float gcu, uint16_t mcsvalue, Mac48Address stadd)
{
  float expected_th = 0;
  std::map<int,float> ::iterator expit;

  if (!isnan(succratio))
{
  PyObject *len1load, *len2load, *len3load, *len4load,*file1args, *file2args, *file3args, *file4args;
  Py_Initialize();
  //std::cout<<"before imprt\n";
  PyRun_SimpleString("from sklearn.externals import joblib\n"
                     "import pandas\n"
                     "import os\n");//pickle and joblib gives the

  PyObject* xdata = PyList_New(2);
  PyList_SET_ITEM(xdata, 0, PyFloat_FromDouble(double(succratio)));
  PyList_SET_ITEM(xdata, 1, PyFloat_FromDouble(double(gcu)));
  PyObject* xdatatup = PyList_New(1);
  PyList_SET_ITEM(xdatatup, 0, xdata);
  //PyObject* objectsRepresentation = PyObject_Repr(xdatatup);
  //const char* s = PyString_AsString(objectsRepresentation);
  //std::cout<<"xdatatup:"<<s<<"\n";
  PyObject* xfeatures = PyList_New(2);
  PyList_SET_ITEM(xfeatures, 0, PyString_FromString("GCU"));
  PyList_SET_ITEM(xfeatures, 1, PyString_FromString("SUCCRATIO"));

  //std::cout<<"xdta size:"<<PyTuple_GET_SIZE(xdata)<<"\n";
  PyObject* pModulepd = PyImport_Import(PyString_FromString((char*)"pandas"));
  PyObject* pFuncpd = PyObject_GetAttrString(pModulepd, (char*)"DataFrame");
  PyObject* args = PyTuple_Pack(1, xdatatup);
  PyObject *keywords = PyDict_New();
  PyDict_SetItemString(keywords, "columns", xfeatures);
  PyObject* xtest = PyObject_Call(pFuncpd, args, keywords);
  PyObject* jblen1load = PyObject_GetAttrString(PyImport_Import(PyString_FromString((char*)"sklearn.externals.joblib")), (char*)"load");
  PyObject* testobj;

  /*PyObject* fileargs = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs0_550.pkl"));
  PyObject* lenload = PyObject_CallObject(jblen1load, fileargs);
  std::cout<<lenload<<" end\n";*/
  /*PyRun_SimpleString( "file = open('/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs0_1024.pkl','rb')\n"
                      "x=joblib.load('/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs0_1024.pkl')\n"//file is not found//pickle.load is loaded wth rght file name
                      "file.close()\n"
                      "print(x)\n");*///only joblib refcount
  //std::cout<<"PVALUE:"<<f<<"\n";
  if (mcsvalue == 0)
  {
    //PyObject* jblen1mod = PyImport_Import(PyString_FromString((char*)"pandas"));
     file1args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs0_550.pkl"));
     len1load = PyObject_CallObject(jblen1load, file1args);
     file2args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs0_1024.pkl"));
     len2load = PyObject_CallObject(jblen1load, file2args);
     file3args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs0_2048.pkl"));
     len3load = PyObject_CallObject(jblen1load, file3args);
     file4args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs0_3839.pkl"));
     len4load = PyObject_CallObject(jblen1load, file4args);

     for(expit = mcs0agglengthexpth.begin(); expit != mcs0agglengthexpth.end(); expit++)
     {
       if(expit->first == 550)
       {
         testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len1load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
         char* mcs0sa550 = PyString_AsString(testobj);
         expected_th =  atof(mcs0sa550);
         //std::cout<<"Expected Th:"<<expected_th<<"\n";
         expit->second = expected_th;
       }

       if(expit->first == 1024)
       {
         testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len2load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
         char* mcs0sa1024 = PyString_AsString(testobj);
         expected_th =  atof(mcs0sa1024);
         //std::cout<<"Expected Th:"<<expected_th<<"\n";
         expit->second = expected_th;
       }

       if(expit->first == 2048)
       {
         testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len3load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
         char* mcs0sa2048 = PyString_AsString(testobj);
         expected_th =  atof(mcs0sa2048);
         //std::cout<<"Expected Th:"<<expected_th<<"\n";
         expit->second = expected_th;
       }

       if(expit->first == 3839)
       {
         testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len4load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
         char* mcs0sa3839 = PyString_AsString(testobj);
         expected_th =  atof(mcs0sa3839);
         //std::cout<<"Expected Th:"<<expected_th<<"\n";
         expit->second = expected_th;
       }
     }
     genagglengthexpth = mcs0agglengthexpth;
  }

  if (mcsvalue == 1)
  {
     file1args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs1_550.pkl"));
     len1load = PyObject_CallObject(jblen1load, file1args);
     file2args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs1_1024.pkl"));
     len2load = PyObject_CallObject(jblen1load, file2args);
     file3args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs1_2048.pkl"));
     len3load = PyObject_CallObject(jblen1load, file3args);
     file4args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs1_3839.pkl"));
     len4load = PyObject_CallObject(jblen1load, file4args);

     for(expit = mcs1agglengthexpth.begin(); expit != mcs1agglengthexpth.end(); expit++)
     {
       if(expit->first == 550)
       {
         testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len1load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
         char* mcs1sa550 = PyString_AsString(testobj);
         expected_th =  atof(mcs1sa550);
         //std::cout<<"Expected Th:"<<expected_th<<"\n";
         expit->second = expected_th;
       }

       if(expit->first == 1024)
       {
         testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len2load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
         char* mcs1sa1024 = PyString_AsString(testobj);
         expected_th =  atof(mcs1sa1024);
         //std::cout<<"Expected Th:"<<expected_th<<"\n";
         expit->second = expected_th;
       }

       if(expit->first == 2048)
       {
         testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len3load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
         char* mcs1sa2048 = PyString_AsString(testobj);
         expected_th =  atof(mcs1sa2048);
         //std::cout<<"Expected Th:"<<expected_th<<"\n";
         expit->second = expected_th;
       }

       if(expit->first == 3839)
       {
         testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len4load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
         char* mcs1sa3839 = PyString_AsString(testobj);
         expected_th =  atof(mcs1sa3839);
         //std::cout<<"Expected Th:"<<expected_th<<"\n";
         expit->second = expected_th;
       }
     }
     genagglengthexpth = mcs1agglengthexpth;
  }

  if (mcsvalue == 2)
  {
     file1args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs2_550.pkl"));
     len1load = PyObject_CallObject(jblen1load, file1args);
     file2args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs2_1024.pkl"));
     len2load = PyObject_CallObject(jblen1load, file2args);
     file3args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs2_2048.pkl"));
     len3load = PyObject_CallObject(jblen1load, file3args);
     file4args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs2_3839.pkl"));
     len4load = PyObject_CallObject(jblen1load, file4args);

     for(expit = mcs2agglengthexpth.begin(); expit != mcs2agglengthexpth.end(); expit++)
     {
       if(expit->first == 550)
       {
         testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len1load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
         char* mcs2sa550 = PyString_AsString(testobj);
         expected_th =  atof(mcs2sa550);
         //std::cout<<"Expected Th:"<<expected_th<<"\n";
         expit->second = expected_th;
       }

       if(expit->first == 1024)
       {
         testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len2load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
         char* mcs2sa1024 = PyString_AsString(testobj);
         expected_th =  atof(mcs2sa1024);
         //std::cout<<"Expected Th:"<<expected_th<<"\n";
         expit->second = expected_th;
       }

       if(expit->first == 2048)
       {
         testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len3load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
         char* mcs2sa2048 = PyString_AsString(testobj);
         expected_th =  atof(mcs2sa2048);
         //std::cout<<"Expected Th:"<<expected_th<<"\n";
         expit->second = expected_th;
       }

       if(expit->first == 3839)
       {
         testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len4load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
         char* mcs2sa3839 = PyString_AsString(testobj);
         expected_th =  atof(mcs2sa3839);
         //std::cout<<"Expected Th:"<<expected_th<<"\n";
         expit->second = expected_th;
       }
     }
     genagglengthexpth = mcs2agglengthexpth;
  }

  if (mcsvalue == 3)
  {
     file1args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs3_550.pkl"));
     len1load = PyObject_CallObject(jblen1load, file1args);
     file2args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs3_1024.pkl"));
     len2load = PyObject_CallObject(jblen1load, file2args);
     file3args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs3_2048.pkl"));
     len3load = PyObject_CallObject(jblen1load, file3args);
     file4args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs3_3839.pkl"));
     len4load = PyObject_CallObject(jblen1load, file4args);

     for(expit = mcs3agglengthexpth.begin(); expit != mcs3agglengthexpth.end(); expit++)
     {
       if(expit->first == 550)
       {
         testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len1load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
         char* mcs3sa550 = PyString_AsString(testobj);
         expected_th =  atof(mcs3sa550);
         //std::cout<<"Expected Th:"<<expected_th<<"\n";
         expit->second = expected_th;
       }

       if(expit->first == 1024)
       {
         testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len2load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
         char* mcs3sa1024 = PyString_AsString(testobj);
         expected_th =  atof(mcs3sa1024);
         //std::cout<<"Expected Th:"<<expected_th<<"\n";
         expit->second = expected_th;
       }

       if(expit->first == 2048)
       {
         testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len3load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
         char* mcs3sa2048 = PyString_AsString(testobj);
         expected_th =  atof(mcs3sa2048);
         //std::cout<<"Expected Th:"<<expected_th<<"\n";
         expit->second = expected_th;
       }

       if(expit->first == 3839)
       {
         testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len4load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
         char* mcs3sa3839 = PyString_AsString(testobj);
         expected_th =  atof(mcs3sa3839);
         //std::cout<<"Expected Th:"<<expected_th<<"\n";
         expit->second = expected_th;
       }
     }
     genagglengthexpth = mcs3agglengthexpth;
  }
  if (mcsvalue == 4)
  {
     file1args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs4_550.pkl"));
     len1load = PyObject_CallObject(jblen1load, file1args);
     file2args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs4_1024.pkl"));
     len2load = PyObject_CallObject(jblen1load, file2args);
     file3args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs4_2048.pkl"));
     len3load = PyObject_CallObject(jblen1load, file3args);
     file4args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs4_3839.pkl"));
     len4load = PyObject_CallObject(jblen1load, file4args);

     for(expit = mcs4agglengthexpth.begin(); expit != mcs4agglengthexpth.end(); expit++)
     {
       if(expit->first == 550)
       {
         testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len1load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
         char* mcs4sa550 = PyString_AsString(testobj);
         expected_th =  atof(mcs4sa550);
         //std::cout<<"Expected Th:"<<expected_th<<"\n";
         expit->second = expected_th;
       }

       if(expit->first == 1024)
       {
         testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len2load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
         char* mcs4sa1024 = PyString_AsString(testobj);
         expected_th =  atof(mcs4sa1024);
         //std::cout<<"Expected Th:"<<expected_th<<"\n";
         expit->second = expected_th;
       }

       if(expit->first == 2048)
       {
         testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len3load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
         char* mcs4sa2048 = PyString_AsString(testobj);
         expected_th =  atof(mcs4sa2048);
         //std::cout<<"Expected Th:"<<expected_th<<"\n";
         expit->second = expected_th;
       }

       if(expit->first == 3839)
       {
         testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len4load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
         char* mcs4sa3839 = PyString_AsString(testobj);
         expected_th =  atof(mcs4sa3839);
         //std::cout<<"Expected Th:"<<expected_th<<"\n";
         expit->second = expected_th;
       }
     }
     genagglengthexpth = mcs4agglengthexpth;
  }
  if (mcsvalue == 5)
  {
     file1args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs5_550.pkl"));
     len1load = PyObject_CallObject(jblen1load, file1args);
     file2args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs5_1024.pkl"));
     len2load = PyObject_CallObject(jblen1load, file2args);
     file3args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs5_2048.pkl"));
     len3load = PyObject_CallObject(jblen1load, file3args);
     file4args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs5_3839.pkl"));
     len4load = PyObject_CallObject(jblen1load, file4args);

     for(expit = mcs5agglengthexpth.begin(); expit != mcs5agglengthexpth.end(); expit++)
     {
       if(expit->first == 550)
       {
         testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len1load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
         char* mcs5sa550 = PyString_AsString(testobj);
         expected_th =  atof(mcs5sa550);
         //std::cout<<"Expected Th:"<<expected_th<<"\n";
         expit->second = expected_th;
       }

       if(expit->first == 1024)
       {
         testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len2load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
         char* mcs5sa1024 = PyString_AsString(testobj);
         expected_th =  atof(mcs5sa1024);
         //std::cout<<"Expected Th:"<<expected_th<<"\n";
         expit->second = expected_th;
       }

       if(expit->first == 2048)
       {
         testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len3load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
         char* mcs5sa2048 = PyString_AsString(testobj);
         expected_th =  atof(mcs5sa2048);
         //std::cout<<"Expected Th:"<<expected_th<<"\n";
         expit->second = expected_th;
       }

       if(expit->first == 3839)
       {
         testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len4load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
         char* mcs5sa3839 = PyString_AsString(testobj);
         expected_th =  atof(mcs5sa3839);
         //std::cout<<"Expected Th:"<<expected_th<<"\n";
         expit->second = expected_th;
       }
     }
     genagglengthexpth = mcs5agglengthexpth;
  }
  if (mcsvalue == 6)
  {
     file1args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs6_550.pkl"));
     len1load = PyObject_CallObject(jblen1load, file1args);
     file2args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs6_1024.pkl"));
     len2load = PyObject_CallObject(jblen1load, file2args);
     file3args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs6_2048.pkl"));
     len3load = PyObject_CallObject(jblen1load, file3args);
     file4args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs6_3839.pkl"));
     len4load = PyObject_CallObject(jblen1load, file4args);

     for(expit = mcs6agglengthexpth.begin(); expit != mcs6agglengthexpth.end(); expit++)
     {
       if(expit->first == 550)
       {
         testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len1load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
         char* mcs6sa550 = PyString_AsString(testobj);
         expected_th =  atof(mcs6sa550);
         //std::cout<<"Expected Th:"<<expected_th<<"\n";
         expit->second = expected_th;
       }

       if(expit->first == 1024)
       {
         testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len2load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
         char* mcs6sa1024 = PyString_AsString(testobj);
         expected_th =  atof(mcs6sa1024);
         //std::cout<<"Expected Th:"<<expected_th<<"\n";
         expit->second = expected_th;
       }

       if(expit->first == 2048)
       {
         testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len3load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
         char* mcs6sa2048 = PyString_AsString(testobj);
         expected_th =  atof(mcs6sa2048);
         //std::cout<<"Expected Th:"<<expected_th<<"\n";
         expit->second = expected_th;
       }

       if(expit->first == 3839)
       {
         testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len4load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
         char* mcs6sa3839 = PyString_AsString(testobj);
         expected_th =  atof(mcs6sa3839);
         //std::cout<<"Expected Th:"<<expected_th<<"\n";
         expit->second = expected_th;
       }
     }
     genagglengthexpth = mcs6agglengthexpth;
  }
  if (mcsvalue == 7)
  {
     file1args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs7_550.pkl"));
     len1load = PyObject_CallObject(jblen1load, file1args);
     file2args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs7_1024.pkl"));
     len2load = PyObject_CallObject(jblen1load, file2args);
     file3args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs7_2048.pkl"));
     len3load = PyObject_CallObject(jblen1load, file3args);
     file4args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs7_3839.pkl"));
     len4load = PyObject_CallObject(jblen1load, file4args);

     for(expit = mcs7agglengthexpth.begin(); expit != mcs7agglengthexpth.end(); expit++)
     {
       if(expit->first == 550)
       {
         testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len1load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
         char* mcs7sa550 = PyString_AsString(testobj);
         expected_th =  atof(mcs7sa550);
         //std::cout<<"Expected Th:"<<expected_th<<"\n";
         expit->second = expected_th;
       }

       if(expit->first == 1024)
       {
         testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len2load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
         char* mcs7sa1024 = PyString_AsString(testobj);
         expected_th =  atof(mcs7sa1024);
         //std::cout<<"Expected Th:"<<expected_th<<"\n";
         expit->second = expected_th;
       }

       if(expit->first == 2048)
       {
         testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len3load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
         char* mcs7sa2048 = PyString_AsString(testobj);
         expected_th =  atof(mcs7sa2048);
         //std::cout<<"Expected Th:"<<expected_th<<"\n";
         expit->second = expected_th;
       }

       if(expit->first == 3839)
       {
         testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len4load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
         char* mcs7sa3839 = PyString_AsString(testobj);
         expected_th =  atof(mcs7sa3839);
         //std::cout<<"Expected Th 3839:"<<expected_th<<"\n";
         expit->second = expected_th;
       }
     }
     genagglengthexpth = mcs7agglengthexpth;
  }

  uint last_counter=0;
  float highestexp = 0;
  int highexpagglength = 0;
  std::map<int, float>::iterator genit;
  for(genit = genagglengthexpth.begin(); genit != genagglengthexpth.end(); genit++)
  {
    if (highestexp < genit->second)
    {
      highestexp = genit->second;
      //std::cout<<"HIGHEST EXP_TH:"<<highestexp<<"\n";
      highexpagglength = genit->first;
    }

  }

  // previousaggregationlengthperstation is used
  previousaggregationlengthperstation = MsduAggregator::getamsdusizeperstation();
  if (!previous_statistics[stadd])
  {
      previous_statistics[stadd] = (historical_bytes[stadd]-previous_historical_bytes[stadd])/(1024*1024);
  }
  else
  {
    second_statistics[stadd] = previous_statistics[stadd];
    previous_statistics[stadd] = (historical_bytes[stadd]-previous_historical_bytes[stadd])/(1024*1024);

    if (previousaggregationlengthperstation[stadd] != highexpagglength)
    {
      last_counter = 0;
    }
    else
    {
      last_counter+=1;
    }
    if ((second_statistics[stadd] - previous_statistics[stadd])/second_statistics[stadd] < -0.15 && previousaggregationlengthperstation[stadd] != highexpagglength)
    {

      if(bannedagglengthcounterperstation[stadd][agglengthtoindexmap[previousaggregationlengthperstation[stadd]]] == 0)
      {
        std::cout<<"\n";
        bannedagglengthcounterperstation[stadd][agglengthtoindexmap[previousaggregationlengthperstation[stadd]]] = 10;

      }
      else
      {
        bannedagglengthcounterperstation[stadd][agglengthtoindexmap[previousaggregationlengthperstation[stadd]]] += 10;
      }
        //.. check if banned has the length
      //else{}
    }

    if((previous_statistics[stadd] - second_statistics[stadd])/previous_statistics[stadd] < -0.15 && previousaggregationlengthperstation[stadd] != highexpagglength)
    {
      if(bannedagglengthcounterperstation[stadd][agglengthtoindexmap[highexpagglength]] == 0)
      {
        std::cout<<"\n";
        bannedagglengthcounterperstation[stadd][agglengthtoindexmap[highexpagglength]] = 10;

      }
      else
      {
        bannedagglengthcounterperstation[stadd][agglengthtoindexmap[highexpagglength]] += 10;
      }
    }
  }


  uint sm_counter = 0;
  std::map<int, float>::iterator afterbanit;
  for(afterbanit = genagglengthexpth.begin(); afterbanit != genagglengthexpth.end(); afterbanit++)
  {
    sm_counter = bannedagglengthcounterperstation[stadd][agglengthtoindexmap[afterbanit->first]];
    if (sm_counter != 0)
    {
      afterbanit->second = (afterbanit->second)-((afterbanit->second)*sm_counter/100);
    }

    if(bannedagglengthcounterperstation[stadd][agglengthtoindexmap[highexpagglength]] > 0 || last_counter >=5)
    {
      bannedagglengthcounterperstation[stadd][agglengthtoindexmap[highexpagglength]]--;

    }
    else
    {
      if (highestexp < afterbanit->second)
      {
        highestexp = afterbanit->second;
        highexpagglength = afterbanit->first;
        if (previousaggregationlengthperstation[stadd] != highexpagglength)
        {
          last_counter = 0;
        }
      }
    }
  }




  Py_DECREF(len1load);
  Py_DECREF(len2load);
  Py_DECREF(len3load);
  Py_DECREF(len4load);
  Py_DECREF(file1args);
  Py_DECREF(file2args);
  Py_DECREF(file3args);
  Py_DECREF(file4args);
  Py_DECREF(testobj);
  //PyList_SET_ITEM(listObj, 2, PyFloat_FromDouble(double(mcsva)));
  /*PyRun_SimpleString("from time import time,ctime\n"
                     "print 'Today is',ctime(time())\n");*/
  //Py_XDECREF(listObj);

  Py_DECREF(xdata);
  Py_DECREF(xdatatup);
  //Py_DECREF(objectsRepresentation);
  Py_DECREF(xfeatures);
  Py_DECREF(pModulepd);
  Py_DECREF(pFuncpd);
  Py_DECREF(args);
  Py_DECREF(keywords);
  Py_DECREF(xtest);
  Py_DECREF(jblen1load);
  //std::cout<<"HIGHEST EXP_TH:"<<highestexp<<"\n";
  //std::cout<<"The expected throughput is "<<highestexp<<" for aggregation length:"<<highexpagglength<<"\n";
  return std::make_tuple(highexpagglength,highestexp);
}
  //Py_Finalize();// this would render the interpreter uesless! hence sigsegv
  return std::make_tuple(0,0);
}

//compare last and current mcs rfr ns3
int MinstrelHtWifiManager::comparelastandcurrentrfrns3 (float succratio, float gcu, uint16_t currmcs, uint16_t lastmcs, Mac48Address stadd)
{
  float difference;
  float agglength = 0;

  //only the exp_th values
  difference = (std::get<1>(rfrmodelns3(succratio, gcu, currmcs, stadd)) - std::get<1>(rfrmodelns3(succratio, gcu, lastmcs, stadd)))/std::get<1>(rfrmodelns3(succratio, gcu, lastmcs, stadd));
  std::cout<<"At the station MAC:"<<stadd<<" the current expected throughput is "<<std::get<1>(rfrmodelns3(succratio, gcu, currmcs, stadd))<<" the last expected throughput is "<<std::get<1>(rfrmodelns3(succratio, gcu, lastmcs, stadd))<< "\n";
  if (difference < 0.05)
  {
    //derive agg length
    //std::cout<<"ad";
    agglength = std::get<0>(rfrmodelns3(succratio, gcu, lastmcs, stadd));
  }
  else
  {
    //std::cout<<"ad";
    agglength = std::get<0>(rfrmodelns3(succratio, gcu, currmcs, stadd));
  }


  return agglength;
}



////M5P model for ns3
std::tuple<int, float> MinstrelHtWifiManager::m5pmodelplatform(float succratio, float gcu, float lastattemptbytes, float minstrel_th, uint16_t mcsvalue, Mac48Address stadd)
{
  float expected_th = 0;

  //std::cout<<"ns3";
  std::map<int,float> ::iterator expit;
  //MCS0
  if (mcsvalue == 0)
  {
    std::cout<<"The MCS value is "<<mcsvalue<<"\n";
    for(expit = mcs0agglengthexpth.begin(); expit != mcs0agglengthexpth.end(); expit++)
    {
      if(expit->first == 550)
      {
        if (lastattemptbytes > 34280 && lastattemptbytes <= 34280)
        {
          expected_th = 1.0012 * lastattemptbytes	+ 3627.4513 * succratio- 4812.4582;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes > 72114)
        {
          expected_th = 0.9695 * lastattemptbytes	+ 177655.9339 * succratio- 172464.2645;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th = 0.748 * lastattemptbytes	- 148.9901;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }

      }

      if(expit->first == 1024)
      {
        if (lastattemptbytes > 59774 && lastattemptbytes <= 137836)
        {
          expected_th = 0.8803 * lastattemptbytes	+ 1251.5725 * succratio	+ 630.2685 *gcu	- 47500.6259;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes > 99304 && lastattemptbytes <= 176264)
        {
          expected_th = 0.7073 * lastattemptbytes	+ 1160.7185 * gcu- 58284.3375;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 3167.5)
        {
          expected_th = 0.0387 * lastattemptbytes	+ 149.9806;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes > 176264)
        {
          expected_th = 1.2983 * lastattemptbytes- 106749.866;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th = 29160.6202;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

      if(expit->first == 2048)
      {
        if (lastattemptbytes > 60172 && lastattemptbytes <= 141607)
        {
          expected_th = 0.9708 * lastattemptbytes+ 1314.4356 * succratio- 309.4258;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes > 100731)
        {
          expected_th =  0.9471 * lastattemptbytes+ 219434.7577 * succratio- 208070.1289;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th = 0.7314 * lastattemptbytes	- 59.7816;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

      if(expit->first == 3839)
      {
        if (lastattemptbytes > 47705 && lastattemptbytes <= 96999)
        {
          expected_th =-9.9177 * lastattemptbytes	+ 0.9247 * lastattemptbytes	+ 50918.9539 * succratio+ 96.9332 * gcu	- 55888.7605;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes > 1811 && lastattemptbytes <= 72065)
        {
          expected_th = -122.5223 * lastattemptbytes+ 0.5664 * lastattemptbytes	+ 21077.6124 *succratio	+ 59.8891 * gcu- 15928.7687;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if(lastattemptbytes <= 49613)
        {
          expected_th = 0.0354 * lastattemptbytes	+ 4691.891 * succratio- 3537.3489;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if(lastattemptbytes <= 201985)
        {
          expected_th = -3311.8903 * lastattemptbytes+ 0.8113 * lastattemptbytes+ 153652.5608 * succratio- 112970.1392;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if(lastattemptbytes > 241488)
        {
          expected_th = 12263.1968 * lastattemptbytes- 0.1461 * lastattemptbytes+ 260172.3685;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th =  49801.0357 * lastattemptbytes- 13257.1456;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

    }
    genagglengthexpth = mcs0agglengthexpth;
  }
  //MCS1
  if (mcsvalue == 1)
  {
    std::cout<<"The MCS value is "<<mcsvalue<<"\n";
    for(expit = mcs1agglengthexpth.begin(); expit != mcs1agglengthexpth.end(); expit++)
    {
      if(expit->first == 550)
      {
        if (lastattemptbytes <= 61140)
        {
          expected_th = 84.1163 * lastattemptbytes+ 0.9876 *lastattemptbytes+ 2117.5537 * succratio	- 2744.4491;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 175984)
        {
          expected_th = 10793.0715 *lastattemptbytes+ 0.9584 * lastattemptbytes	+ 4868.1258 * succratio	- 93863.7768;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th = 0.9763 * lastattemptbytes	+ 246649.7607 * succratio	- 240885.9307;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

      if(expit->first == 1024)
      {
        if (lastattemptbytes > 201908)
        {
          expected_th = 0.9743 * lastattemptbytes	+ 350566.3588 * succratio	- 341457.277;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes > 3671.5 && lastattemptbytes<=122032)
        {
          expected_th = 0.968 * lastattemptbytes+ 158696.4981 * succratio+ 4.7525 * gcu- 154351.5465;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes<=3671.5)
        {
          expected_th = 0.0338 * lastattemptbytes	+ 265.8779;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th = 0.8819 * lastattemptbytes	+ 5705.4294;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

      if(expit->first == 2048)
      {
        if (lastattemptbytes <= 249287)
        {
          expected_th = 0.9885 * lastattemptbytes	+ 3297.5379 * succratio	- 3087.9201;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th = 0.9668 * lastattemptbytes	+ 392429.5786 * succratio- 380089.8137;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

      if(expit->first == 3839)
      {
        if (lastattemptbytes <=171535 && lastattemptbytes>6642)
        {
          expected_th = 0.9703 * lastattemptbytes	+ 115289.3458 *succratio- 112016.74;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes > 88719 && lastattemptbytes <= 246645)
        {
          expected_th = 0.9601 * lastattemptbytes	+ 229655.7694 * succratio- 220046.0186;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 126274)
        {
          expected_th = 0.038 * lastattemptbytes+ 10033.5784 * succratio- 8796.3036;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes>395553)
        {
          expected_th = 0.633 * lastattemptbytes+ 1021227.2153 * succratio- 834029.7453;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th =  0.8092 * lastattemptbytes+ 301880.9649 * succratio	- 237755.6497;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

    }

    genagglengthexpth = mcs1agglengthexpth;

  }
  //MCS2
  if (mcsvalue == 2)
  {
    std::cout<<"The MCS value is "<<mcsvalue<<"\n";
    for(expit = mcs2agglengthexpth.begin(); expit != mcs2agglengthexpth.end(); expit++)
    {
      if(expit->first == 550)
      {
        if (lastattemptbytes <=153366 && lastattemptbytes <= 3923.5)
        {
          expected_th = 0.0364 * lastattemptbytes+ 1642.3555 * succratio- 1373.4996;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 172902)
        {
          expected_th = 421.221 * lastattemptbytes+ 0.9642 * lastattemptbytes- 3531.813;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th =34730.6538 * lastattemptbytes+ 0.9891 *lastattemptbytes- 415697.2236;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

      if(expit->first == 1024)
      {
        if (lastattemptbytes > 3968 && lastattemptbytes > 328917)
        {
          expected_th = 0.9828 * lastattemptbytes	+ 430534.8275 * succratio	- 422662.4859;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 3968 )
        {
          expected_th = 0.0249 * lastattemptbytes	+ 1726.6751 * succratio	- 1433.6429;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 162586)
        {
          expected_th = 0.996 * lastattemptbytes+ 10062.4674 * succratio- 10072.2468;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 219779)
        {
          expected_th = 0.9895 * lastattemptbytes	+ 13692.3711 * succratio- 12493.2365;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th =0.9185 * lastattemptbytes+ 269778.9299 * succratio	- 248366.0629;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

      if(expit->first == 2048)
      {
        if (lastattemptbytes<=302749)
        {
          expected_th = 364.0363 * lastattemptbytes+ 0.9932 * lastattemptbytes- 4332.8353;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th =  46325.3101 * lastattemptbytes+ 0.9822 * lastattemptbytes	- 551094.1685;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

      if(expit->first == 3839)
      {
        if (lastattemptbytes <= 271005 && lastattemptbytes > 9704.5)
        {
          expected_th = 0.9755 * lastattemptbytes	+ 11749.9738 * succratio- 11293.0603;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 138609.5)
        {
          expected_th = 0.0348 * lastattemptbytes	+ 11113.8254 * succratio- 10272.1386;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 359310)
        {
          expected_th = 0.9607 * lastattemptbytes	+ 316741.2249 * succratio	- 303848.0974;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th =  0.7431 * lastattemptbytes+ 609286.362 * succratio- 446990.9487;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

    }
    genagglengthexpth = mcs2agglengthexpth;
  }
  //MCS3
  if (mcsvalue == 3)
  {
    std::cout<<"The MCS value is "<<mcsvalue<<"\n";
    for(expit = mcs3agglengthexpth.begin(); expit != mcs3agglengthexpth.end(); expit++)
    {
      if(expit->first == 550)
      {
          expected_th = 0.9672 * lastattemptbytes- 4.7476;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
      }

      if(expit->first == 1024)
      {
        if (lastattemptbytes > 4011 && lastattemptbytes > 296532)
        {
          expected_th =  0.9773 * lastattemptbytes	+ 482070.1707 * succratio- 49.1777 * gcu- 468461.0774;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th = 0.9883 * lastattemptbytes	+ 98.3863;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

      if(expit->first == 2048)
      {

        if (lastattemptbytes > 3489 && lastattemptbytes > 399289)
        {
          expected_th = 0.9842 * lastattemptbytes	+ 4.4051 * gcu+ 1007.3399;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 202128)
        {
          expected_th = 0.0336 * lastattemptbytes+ 5.315 * gcu- 191.1861;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 799678)
        {
          expected_th = 0.7378 * lastattemptbytes+ 752.755 * gcu+ 64994.3402;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th =  1.0624 * lastattemptbytes- 91866.1708;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }

      }

      if(expit->first == 3839)
      {
        if (lastattemptbytes > 6335 && lastattemptbytes > 299889)
        {
          expected_th = 0.9702 * lastattemptbytes	+ 15411.4263 * succratio- 13454.9991;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 153632)
        {
          expected_th =0.0321 * lastattemptbytes+ 12046.6541 * succratio- 11209.1007;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 527511)
        {
          expected_th = 0.9324 * lastattemptbytes	+ 439783.4012 * succratio- 411025.7115;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th = 0.7903 * lastattemptbytes	+ 876479.1705 * succratio- 716486.5348;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

    }
    genagglengthexpth = mcs3agglengthexpth;
  }
  //MCS4
  if (mcsvalue == 4)
  {
    std::cout<<"The MCS value is "<<mcsvalue<<"\n";
    for(expit = mcs4agglengthexpth.begin(); expit != mcs4agglengthexpth.end(); expit++)
    {
      if(expit->first == 550)
      {
          expected_th = 0.9485 * lastattemptbytes	+ 310.9914;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
      }

      if(expit->first == 1024)
      {

        if (lastattemptbytes > 199759)
        {
          expected_th = 0.9784 * lastattemptbytes	+ 530856.6155 *succratio- 194.9456 * gcu- 508980.2825;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 2231.5)
        {
          expected_th = 0.023 * lastattemptbytes+ 1246.484 * succratio- 996.0992;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 106426)
        {
          expected_th = 0.8624 * lastattemptbytes+ 61669.3648 * succratio	- 53298.0278;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th =  0.9089 * lastattemptbytes+ 139552.2598 * succratio- 127232.2565;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }

      }

      if(expit->first == 2048)
      {

        if (lastattemptbytes > 320550)
        {
          expected_th = 0.9767 * lastattemptbytes	+ 611791.206 * succratio- 596780.9409;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes  > 3320)
        {
          expected_th = 0.8957 * lastattemptbytes	+ 148562.5908 *succratio+ 209.4038 * gcu- 138577.494;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th =  0.7635 * lastattemptbytes+ 37.7275;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }

      }

      if(expit->first == 3839)
      {
        if (lastattemptbytes >142691)
        {
          expected_th =0.965 * lastattemptbytes	+ 678434.223 * succratio- 656095.0864;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 5740)
        {
          expected_th = 36.7155 * lastattemptbytes+ 0.0316 * lastattemptbytes	- 478.5797;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th =  3898.7353 * lastattemptbytes	+ 0.9661 * lastattemptbytes- 75941.1824;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

    }
    genagglengthexpth = mcs4agglengthexpth;
  }
  //MCS5
  if (mcsvalue == 5)
  {
    std::cout<<"The MCS value is "<<mcsvalue<<"\n";
    for(expit = mcs5agglengthexpth.begin(); expit != mcs5agglengthexpth.end(); expit++)
    {
      if(expit->first == 550)
      {
          expected_th =  0.971 * lastattemptbytes	+ 49419.0645 * succratio	- 47274.1827;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
      }

      if(expit->first == 1024)
      {

        if (lastattemptbytes > 204548 && lastattemptbytes > 3422)
        {
          expected_th = 0.8859 * lastattemptbytes	+ 68974.2735 * succratio- 58946.9454;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 104356)
        {
          expected_th = 0.0232 * lastattemptbytes	+ 6059.0139 *succratio- 5444.3316;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 458520)
        {
          expected_th = 0.9567 * lastattemptbytes+ 414732.7451 * succratio- 399890.0929;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th =  0.9787 * lastattemptbytes+ 692131.8237 * succratio	- 675296.4564;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }

      }

      if(expit->first == 2048)
      {
        if (lastattemptbytes > 8338 && lastattemptbytes <= 216869)
        {
          expected_th = 0.8225 * lastattemptbytes+ 98160.1931 * succratio+ 1.0336 * gcu	- 79199.8743;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes > 112423 && lastattemptbytes <= 589758)
        {
          expected_th = 0.9595 * lastattemptbytes	+ 847.6563 * gcu- 40580.1413;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 299784)
        {
          expected_th =0.0357 * lastattemptbytes+ 211.3386;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th = 0.994 * lastattemptbytes- 18642.3685;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

      if(expit->first == 3839)
      {
        if (lastattemptbytes <= 177938 && lastattemptbytes <= 1908)
        {
          expected_th = 0.0509 * lastattemptbytes	+ 7014.5379 * succratio- 6547.4092;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 479080 && lastattemptbytes > 177938)
        {
          expected_th = -3265.7632 * lastattemptbytes	+ 0.9611 * lastattemptbytes+ 346870.5062 *succratio- 256843.6957;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 327845)
        {
          expected_th = 0.8723 * lastattemptbytes	+ 24862.7084 * succratio+ 6.3024 * gcu- 19647.0595;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 1246482 && lastattemptbytes > 575707)
        {
          expected_th = 0.8235 * lastattemptbytes	+ 843712.515 * succratio+ 373.814 * gcu	- 737772.9139;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }

        else
        {
          expected_th =   0.9941 *lastattemptbytes- 7313.4644;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

    }
    genagglengthexpth = mcs5agglengthexpth;
  }
  //MCS6
  if (mcsvalue == 6)
  {
    std::cout<<"The MCS value is "<<mcsvalue<<"\n";
    for(expit = mcs6agglengthexpth.begin(); expit != mcs6agglengthexpth.end(); expit++)
    {
      if(expit->first == 550)
      {
        if (lastattemptbytes <= 61166 && lastattemptbytes > 1627)
        {
          expected_th = 23.4058 * lastattemptbytes+ 1.0016 * lastattemptbytes	- 4198.8261;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 31501 )
        {
          expected_th = 140.8471 * lastattemptbytes	+ 0.0247 * lastattemptbytes- 3368.5203;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes > 300840)
        {
          expected_th = 21186.5374 * lastattemptbytes	+ 0.9778 * lastattemptbytes	- 532906.3374;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th = 0.7978 * lastattemptbytes	+ 15493.9293;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

      if(expit->first == 1024)
      {

            if (lastattemptbytes <= 171652 && lastattemptbytes > 3335 && lastattemptbytes <= 8096)
            {
              expected_th = 0.8785 * lastattemptbytes	+ 1666.9202 * succratio- 5162.5654;
              std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
              expit->second = expected_th;
            }
            if (lastattemptbytes <= 42240 )
            {
              expected_th = 0.0237 * lastattemptbytes	+ 7147.9122 * succratio	- 6544.7963;
              std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
              expit->second = expected_th;
            }
            if (lastattemptbytes <= 367548 && lastattemptbytes > 200688)
            {
              expected_th = 9562.3079 * lastattemptbytes+ 0.9623 * lastattemptbytes	+ 13913.1298 * succratio- 248963.4433;
              std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
              expit->second = expected_th;
            }
            if (lastattemptbytes <= 284111)
            {
              expected_th = 0.9617 * lastattemptbytes	+ 16688.7637 * succratio- 17820.9211;
              std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
              expit->second = expected_th;
            }
            if (lastattemptbytes <= 926488)
            {
              expected_th = 0.8334 * lastattemptbytes	+ 626530.8477 * succratio	- 535139.6843;
              std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
              expit->second = expected_th;
            }
            else
            {
              expected_th =  0.9797 * lastattemptbytes+ 4861.3531;
              std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
              expit->second = expected_th;
            }

      }

      if(expit->first == 2048)
      {
        if (lastattemptbytes > 161958)
        {
          expected_th = -9260.9131 * lastattemptbytes	+ 0.9708 *lastattemptbytes+ 779952.7734 *succratio-523359.9254;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 3298 )
        {
          expected_th = 0.0308 * lastattemptbytes	+ 1726.6335 *succratio- 2.9828 * gcu- 1253.645;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes > 100218)
        {
          expected_th =0.8444 * lastattemptbytes+ 117836.9414 * succratio+ 5.3214 *gcu- 99717.4501;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 59476 && lastattemptbytes > 34282)
        {
          expected_th = 0.7344 *lastattemptbytes+ 47363.937 *succratio- 34965.1629;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (succratio <= 0.709 && lastattemptbytes > 60624)
        {
          expected_th = 0.6245 * lastattemptbytes	+ 80036.2109 *succratio- 49914.1271;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes > 46782)
        {
          expected_th = 0.8563 * lastattemptbytes	+ 67385.3781 * succratio- 57890.2951;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th =  0.6977 * lastattemptbytes+ 1551.5932;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

      if(expit->first == 3839)
      {
        if (lastattemptbytes <= 582636 && lastattemptbytes >122502)
        {
          expected_th = 0.9558 * lastattemptbytes+ 305963.2834 *succratio	+ 56.5189 * gcu- 295397.3707;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }

        if (lastattemptbytes <= 352543)
        {
          expected_th = 0.9679 * lastattemptbytes+ 10630.9312 * succratio	- 10315.7868;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th =  0.971 * lastattemptbytes	+ 942565.6878 *succratio+ 281.3889 *gcu- 930400.2033;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

    }
    genagglengthexpth = mcs6agglengthexpth;
  }
  //MCS7
  if (mcsvalue == 7)
  {
    std::cout<<"The MCS value is "<<mcsvalue<<"\n";
    for(expit = mcs7agglengthexpth.begin(); expit != mcs7agglengthexpth.end(); expit++)
    {
      if(expit->first == 550)
      {
        if (lastattemptbytes <= 71484 && lastattemptbytes > 1790)
        {
          expected_th = 1.006 * lastattemptbytes+ 625.0678 *succratio	+ 2.1483 *gcu	- 4395.3437;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 36911)
        {
          expected_th = 0.0233 * lastattemptbytes	+ 2185.3303 *succratio+ 3.48 * gcu- 1956.8102;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th = 0.9629 * lastattemptbytes	+ 324594.597 *succratio+ 373.6267 * gcu- 320463.531;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

      if(expit->first == 1024)
      {
        if (lastattemptbytes <= 204300 && lastattemptbytes > 3228)
        {
          expected_th = -28.8594 * lastattemptbytes+ 0.9165 *lastattemptbytes+ 61115.0822 *succratio- 53038.9563;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 103818)
        {
          expected_th = 0.0246 * lastattemptbytes	+ 6875.8503 *succratio- 6404.1852;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 823228 && lastattemptbytes <= 340236)
        {
          expected_th = -92.5032 * lastattemptbytes	+ 0.997 *lastattemptbytes+ 58707.1973 *succratio- 58475.3998;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th = -2015.1501 * lastattemptbytes	+ 0.9802 * lastattemptbytes	+ 705177.8561 *succratio- 637704.6921;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }

      }

      if(expit->first == 2048)
      {
        if (lastattemptbytes > 215140)
        {
          expected_th = 0.9847 * lastattemptbytes	+ 970053.6998 *succratio- 958352.4903;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 3307)
        {
          expected_th =0.0307 * lastattemptbytes+ 1672.7705 *succratio- 1347.6581;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 105648 && lastattemptbytes > 58342)
        {
          expected_th = 0.7159 * lastattemptbytes	+ 71980.7607 *succratio- 51997.0097;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes > 81994)
        {
          expected_th = 0.9182 *lastattemptbytes+ 132421.7567 *succratio- 120883.7946;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th = 0.781 * lastattemptbytes+ 38864.9208 *succratio- 31018.4872;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

      if(expit->first == 3839)
      {
        if (lastattemptbytes <= 150471)
        {
          expected_th = 0.9745 *lastattemptbytes+ 9490.867 *succratio	- 9361.1607;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 700755 && lastattemptbytes > 377093)
        {
          expected_th = 0.9392 * lastattemptbytes	+ 604756.0313 * succratio- 133.5164 * gcu- 563017.9272;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 537868)
        {
          expected_th = 0.9346 *lastattemptbytes+ 48633.9 *succratio- 34588.06;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        if (lastattemptbytes <= 1111286)
        {
          expected_th = 0.8931 * lastattemptbytes+ 924526.5434 *succratio	- 828108.3156;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
        else
        {
          expected_th = 0.9494 *lastattemptbytes+ 1794047.6379 * succratio- 1708470.0128;
          std::cout<<"The expected throughput is "<<expected_th<<" for aggregation length:"<<expit->first<<"\n";
          expit->second = expected_th;
        }
      }

    }
    genagglengthexpth = mcs7agglengthexpth;
  }

  //find the max exp_th from the 4 scenarios
  uint last_counter=0;
  float highestexp = 0;
  int highexpagglength = 0;




  std::map<int, float>::iterator genit;
  for(genit = genagglengthexpth.begin(); genit != genagglengthexpth.end(); genit++)
  {
    if (highestexp < genit->second)
    {
      highestexp = genit->second;
      highexpagglength = genit->first;
    }

  }

  // previousaggregationlengthperstation is used
  previousaggregationlengthperstation = MsduAggregator::getamsdusizeperstation();
  if (!previous_statistics[stadd])
  {
      previous_statistics[stadd] = (historical_bytes[stadd]-previous_historical_bytes[stadd])/(1024*1024);
  }
  else
  {
    second_statistics[stadd] = previous_statistics[stadd];
    previous_statistics[stadd] = (historical_bytes[stadd]-previous_historical_bytes[stadd])/(1024*1024);

    if (previousaggregationlengthperstation[stadd] != highexpagglength)
    {
      last_counter = 0;
    }
    else
    {
      last_counter+=1;
    }
    if ((second_statistics[stadd] - previous_statistics[stadd])/second_statistics[stadd] < -0.15 && previousaggregationlengthperstation[stadd] != highexpagglength)
    {

      if(bannedagglengthcounterperstation[stadd][agglengthtoindexmap[previousaggregationlengthperstation[stadd]]] == 0)
      {
        std::cout<<"\n";
        bannedagglengthcounterperstation[stadd][agglengthtoindexmap[previousaggregationlengthperstation[stadd]]] = 10;

      }
      else
      {
        bannedagglengthcounterperstation[stadd][agglengthtoindexmap[previousaggregationlengthperstation[stadd]]] += 10;
      }
        //.. check if banned has the length
      //else{}
    }

    if((previous_statistics[stadd] - second_statistics[stadd])/previous_statistics[stadd] < -0.15 && previousaggregationlengthperstation[stadd] != highexpagglength)
    {
      if(bannedagglengthcounterperstation[stadd][agglengthtoindexmap[highexpagglength]] == 0)
      {
        std::cout<<"\n";
        bannedagglengthcounterperstation[stadd][agglengthtoindexmap[highexpagglength]] = 10;

      }
      else
      {
        bannedagglengthcounterperstation[stadd][agglengthtoindexmap[highexpagglength]] += 10;
      }
    }
  }


  uint sm_counter = 0;
  std::map<int, float>::iterator afterbanit;
  for(afterbanit = genagglengthexpth.begin(); afterbanit != genagglengthexpth.end(); afterbanit++)
  {
    sm_counter = bannedagglengthcounterperstation[stadd][agglengthtoindexmap[afterbanit->first]];
    if (sm_counter != 0)
    {
      afterbanit->second = (afterbanit->second)-((afterbanit->second)*sm_counter/100);
    }

    if(bannedagglengthcounterperstation[stadd][agglengthtoindexmap[highexpagglength]] > 0 || last_counter >=5)
    {
      bannedagglengthcounterperstation[stadd][agglengthtoindexmap[highexpagglength]]--;

    }
    else
    {
      if (highestexp < afterbanit->second)
      {
        highestexp = afterbanit->second;
        highexpagglength = afterbanit->first;
        if (previousaggregationlengthperstation[stadd] != highexpagglength)
        {
          last_counter = 0;
        }
      }
    }


  }


  //int lastagglength = highexpagglength;

  std::cout<<"The expected throughput is "<<highestexp<<" for aggregation length:"<<highexpagglength<<"\n";
  return std::make_tuple(highexpagglength,highestexp);

}



int MinstrelHtWifiManager::comparelastandcurrentplatform (float succratio, float gcu, float lastattemptbytes, float minstrel_th, uint16_t currmcs, uint16_t lastmcs, Mac48Address stadd)
  {
    float difference;
    float agglength = 0;

    //only the exp_th values
    difference = (std::get<1>(m5pmodelplatform(succratio, gcu, lastattemptbytes, minstrel_th, currmcs, stadd)) - std::get<1>(m5pmodelplatform(succratio, gcu, lastattemptbytes, minstrel_th, lastmcs, stadd)))/std::get<1>(m5pmodelplatform(succratio, gcu, lastattemptbytes, minstrel_th, lastmcs, stadd));
    std::cout<<"At the station MAC:"<<stadd<<" the current expected throughput is "<<std::get<1>(m5pmodelplatform(succratio, gcu, lastattemptbytes, minstrel_th, currmcs, stadd))<<" the last expected throughput is "<<std::get<1>(m5pmodelplatform(succratio, gcu, lastattemptbytes, minstrel_th, lastmcs, stadd))<< "\n";
    if (difference < 0.05)
    {
      //derive agg length
      //std::cout<<"ad";
      agglength = std::get<0>(m5pmodelplatform(succratio, gcu, lastattemptbytes, minstrel_th, lastmcs, stadd));
    }
    else
    {
      //std::cout<<"ad";
      agglength = std::get<0>(m5pmodelplatform(succratio, gcu, lastattemptbytes, minstrel_th, currmcs, stadd));
    }


    return agglength;
  }

  ////RFR model for ns3
  std::tuple<int, float> MinstrelHtWifiManager::rfrmodelplatform (float succratio, float gcu, float lastattemptbytes, float minstrel_th, uint16_t mcsvalue, Mac48Address stadd)
  {
    float expected_th = 0;
    std::map<int,float> ::iterator expit;

    if (!isnan(succratio))
  {
    PyObject *len1load, *len2load, *len3load, *len4load,*file1args, *file2args, *file3args, *file4args;
    Py_Initialize();
    //std::cout<<"before imprt\n";
    PyRun_SimpleString("from sklearn.externals import joblib\n"
                       "import pandas\n"
                       "import os\n");//pickle and joblib gives the

    PyObject* xdata = PyList_New(4);
    PyList_SET_ITEM(xdata, 0, PyFloat_FromDouble(double(minstrel_th)));
    PyList_SET_ITEM(xdata, 1, PyFloat_FromDouble(double(lastattemptbytes)));
    PyList_SET_ITEM(xdata, 2, PyFloat_FromDouble(double(succratio)));
    PyList_SET_ITEM(xdata, 3, PyFloat_FromDouble(double(gcu)));
    PyObject* xdatatup = PyList_New(1);
    PyList_SET_ITEM(xdatatup, 0, xdata);
    //PyObject* objectsRepresentation = PyObject_Repr(xdatatup);
    //const char* s = PyString_AsString(objectsRepresentation);
    //std::cout<<"xdatatup:"<<s<<"\n";
    PyObject* xfeatures = PyList_New(4);
    PyList_SET_ITEM(xfeatures, 0, PyString_FromString("Mins_Th"));
    PyList_SET_ITEM(xfeatures, 1, PyString_FromString("Last_Att_Bytes"));
    PyList_SET_ITEM(xfeatures, 2, PyString_FromString("SUCCRATIO"));
    PyList_SET_ITEM(xfeatures, 3, PyString_FromString("Gcu"));

    //std::cout<<"xdta size:"<<PyTuple_GET_SIZE(xdata)<<"\n";
    PyObject* pModulepd = PyImport_Import(PyString_FromString((char*)"pandas"));
    PyObject* pFuncpd = PyObject_GetAttrString(pModulepd, (char*)"DataFrame");
    PyObject* args = PyTuple_Pack(1, xdatatup);
    PyObject *keywords = PyDict_New();
    PyDict_SetItemString(keywords, "columns", xfeatures);
    PyObject* xtest = PyObject_Call(pFuncpd, args, keywords);
    PyObject* jblen1load = PyObject_GetAttrString(PyImport_Import(PyString_FromString((char*)"sklearn.externals.joblib")), (char*)"load");
    PyObject* testobj;

    /*PyObject* fileargs = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs0_550.pkl"));
    PyObject* lenload = PyObject_CallObject(jblen1load, fileargs);
    std::cout<<lenload<<" end\n";*/
    /*PyRun_SimpleString( "file = open('/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs0_1024.pkl','rb')\n"
                        "x=joblib.load('/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFns3/mcs0_1024.pkl')\n"//file is not found//pickle.load is loaded wth rght file name
                        "file.close()\n"
                        "print(x)\n");*///only joblib refcount
    //std::cout<<"PVALUE:"<<f<<"\n";
    if (mcsvalue == 0)
    {
      //PyObject* jblen1mod = PyImport_Import(PyString_FromString((char*)"pandas"));
       file1args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFplatform/mcs0_550.pkl"));
       len1load = PyObject_CallObject(jblen1load, file1args);
       file2args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFplatform/mcs0_1024.pkl"));
       len2load = PyObject_CallObject(jblen1load, file2args);
       file3args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFplatform/mcs0_2048.pkl"));
       len3load = PyObject_CallObject(jblen1load, file3args);
       file4args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFplatform/mcs0_3839.pkl"));
       len4load = PyObject_CallObject(jblen1load, file4args);

       for(expit = mcs0agglengthexpth.begin(); expit != mcs0agglengthexpth.end(); expit++)
       {
         if(expit->first == 550)
         {
           testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len1load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
           char* mcs0sa550 = PyString_AsString(testobj);
           expected_th =  atof(mcs0sa550);
           //std::cout<<"Expected Th:"<<expected_th<<"\n";
           expit->second = expected_th;
         }

         if(expit->first == 1024)
         {
           testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len2load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
           char* mcs0sa1024 = PyString_AsString(testobj);
           expected_th =  atof(mcs0sa1024);
           //std::cout<<"Expected Th:"<<expected_th<<"\n";
           expit->second = expected_th;
         }

         if(expit->first == 2048)
         {
           testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len3load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
           char* mcs0sa2048 = PyString_AsString(testobj);
           expected_th =  atof(mcs0sa2048);
           //std::cout<<"Expected Th:"<<expected_th<<"\n";
           expit->second = expected_th;
         }

         if(expit->first == 3839)
         {
           testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len4load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
           char* mcs0sa3839 = PyString_AsString(testobj);
           expected_th =  atof(mcs0sa3839);
           //std::cout<<"Expected Th:"<<expected_th<<"\n";
           expit->second = expected_th;
         }
       }
       genagglengthexpth = mcs0agglengthexpth;
    }

    if (mcsvalue == 1)
    {
       file1args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFplatform/mcs1_550.pkl"));
       len1load = PyObject_CallObject(jblen1load, file1args);
       file2args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFplatform/mcs1_1024.pkl"));
       len2load = PyObject_CallObject(jblen1load, file2args);
       file3args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFplatform/mcs1_2048.pkl"));
       len3load = PyObject_CallObject(jblen1load, file3args);
       file4args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFplatform/mcs1_3839.pkl"));
       len4load = PyObject_CallObject(jblen1load, file4args);

       for(expit = mcs1agglengthexpth.begin(); expit != mcs1agglengthexpth.end(); expit++)
       {
         if(expit->first == 550)
         {
           testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len1load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
           char* mcs1sa550 = PyString_AsString(testobj);
           expected_th =  atof(mcs1sa550);
           //std::cout<<"Expected Th:"<<expected_th<<"\n";
           expit->second = expected_th;
         }

         if(expit->first == 1024)
         {
           testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len2load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
           char* mcs1sa1024 = PyString_AsString(testobj);
           expected_th =  atof(mcs1sa1024);
           //std::cout<<"Expected Th:"<<expected_th<<"\n";
           expit->second = expected_th;
         }

         if(expit->first == 2048)
         {
           testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len3load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
           char* mcs1sa2048 = PyString_AsString(testobj);
           expected_th =  atof(mcs1sa2048);
           //std::cout<<"Expected Th:"<<expected_th<<"\n";
           expit->second = expected_th;
         }

         if(expit->first == 3839)
         {
           testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len4load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
           char* mcs1sa3839 = PyString_AsString(testobj);
           expected_th =  atof(mcs1sa3839);
           //std::cout<<"Expected Th:"<<expected_th<<"\n";
           expit->second = expected_th;
         }
       }
       genagglengthexpth = mcs1agglengthexpth;
    }

    if (mcsvalue == 2)
    {
       file1args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFplatform/mcs2_550.pkl"));
       len1load = PyObject_CallObject(jblen1load, file1args);
       file2args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFplatform/mcs2_1024.pkl"));
       len2load = PyObject_CallObject(jblen1load, file2args);
       file3args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFplatform/mcs2_2048.pkl"));
       len3load = PyObject_CallObject(jblen1load, file3args);
       file4args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFplatform/mcs2_3839.pkl"));
       len4load = PyObject_CallObject(jblen1load, file4args);

       for(expit = mcs2agglengthexpth.begin(); expit != mcs2agglengthexpth.end(); expit++)
       {
         if(expit->first == 550)
         {
           testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len1load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
           char* mcs2sa550 = PyString_AsString(testobj);
           expected_th =  atof(mcs2sa550);
           //std::cout<<"Expected Th:"<<expected_th<<"\n";
           expit->second = expected_th;
         }

         if(expit->first == 1024)
         {
           testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len2load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
           char* mcs2sa1024 = PyString_AsString(testobj);
           expected_th =  atof(mcs2sa1024);
           //std::cout<<"Expected Th:"<<expected_th<<"\n";
           expit->second = expected_th;
         }

         if(expit->first == 2048)
         {
           testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len3load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
           char* mcs2sa2048 = PyString_AsString(testobj);
           expected_th =  atof(mcs2sa2048);
           //std::cout<<"Expected Th:"<<expected_th<<"\n";
           expit->second = expected_th;
         }

         if(expit->first == 3839)
         {
           testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len4load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
           char* mcs2sa3839 = PyString_AsString(testobj);
           expected_th =  atof(mcs2sa3839);
           //std::cout<<"Expected Th:"<<expected_th<<"\n";
           expit->second = expected_th;
         }
       }
       genagglengthexpth = mcs2agglengthexpth;
    }

    if (mcsvalue == 3)
    {
       file1args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFplatform/mcs3_550.pkl"));
       len1load = PyObject_CallObject(jblen1load, file1args);
       file2args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFplatform/mcs3_1024.pkl"));
       len2load = PyObject_CallObject(jblen1load, file2args);
       file3args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFplatform/mcs3_2048.pkl"));
       len3load = PyObject_CallObject(jblen1load, file3args);
       file4args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFplatform/mcs3_3839.pkl"));
       len4load = PyObject_CallObject(jblen1load, file4args);

       for(expit = mcs3agglengthexpth.begin(); expit != mcs3agglengthexpth.end(); expit++)
       {
         if(expit->first == 550)
         {
           testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len1load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
           char* mcs3sa550 = PyString_AsString(testobj);
           expected_th =  atof(mcs3sa550);
           //std::cout<<"Expected Th:"<<expected_th<<"\n";
           expit->second = expected_th;
         }

         if(expit->first == 1024)
         {
           testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len2load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
           char* mcs3sa1024 = PyString_AsString(testobj);
           expected_th =  atof(mcs3sa1024);
           //std::cout<<"Expected Th:"<<expected_th<<"\n";
           expit->second = expected_th;
         }

         if(expit->first == 2048)
         {
           testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len3load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
           char* mcs3sa2048 = PyString_AsString(testobj);
           expected_th =  atof(mcs3sa2048);
           //std::cout<<"Expected Th:"<<expected_th<<"\n";
           expit->second = expected_th;
         }

         if(expit->first == 3839)
         {
           testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len4load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
           char* mcs3sa3839 = PyString_AsString(testobj);
           expected_th =  atof(mcs3sa3839);
           //std::cout<<"Expected Th:"<<expected_th<<"\n";
           expit->second = expected_th;
         }
       }
       genagglengthexpth = mcs3agglengthexpth;
    }
    if (mcsvalue == 4)
    {
       file1args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFplatform/mcs4_550.pkl"));
       len1load = PyObject_CallObject(jblen1load, file1args);
       file2args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFplatform/mcs4_1024.pkl"));
       len2load = PyObject_CallObject(jblen1load, file2args);
       file3args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFplatform/mcs4_2048.pkl"));
       len3load = PyObject_CallObject(jblen1load, file3args);
       file4args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFplatform/mcs4_3839.pkl"));
       len4load = PyObject_CallObject(jblen1load, file4args);

       for(expit = mcs4agglengthexpth.begin(); expit != mcs4agglengthexpth.end(); expit++)
       {
         if(expit->first == 550)
         {
           testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len1load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
           char* mcs4sa550 = PyString_AsString(testobj);
           expected_th =  atof(mcs4sa550);
           //std::cout<<"Expected Th:"<<expected_th<<"\n";
           expit->second = expected_th;
         }

         if(expit->first == 1024)
         {
           testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len2load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
           char* mcs4sa1024 = PyString_AsString(testobj);
           expected_th =  atof(mcs4sa1024);
           //std::cout<<"Expected Th:"<<expected_th<<"\n";
           expit->second = expected_th;
         }

         if(expit->first == 2048)
         {
           testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len3load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
           char* mcs4sa2048 = PyString_AsString(testobj);
           expected_th =  atof(mcs4sa2048);
           //std::cout<<"Expected Th:"<<expected_th<<"\n";
           expit->second = expected_th;
         }

         if(expit->first == 3839)
         {
           testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len4load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
           char* mcs4sa3839 = PyString_AsString(testobj);
           expected_th =  atof(mcs4sa3839);
           //std::cout<<"Expected Th:"<<expected_th<<"\n";
           expit->second = expected_th;
         }
       }
       genagglengthexpth = mcs4agglengthexpth;
    }
    if (mcsvalue == 5)
    {
       file1args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFplatform/mcs5_550.pkl"));
       len1load = PyObject_CallObject(jblen1load, file1args);
       file2args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFplatform/mcs5_1024.pkl"));
       len2load = PyObject_CallObject(jblen1load, file2args);
       file3args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFplatform/mcs5_2048.pkl"));
       len3load = PyObject_CallObject(jblen1load, file3args);
       file4args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFplatform/mcs5_3839.pkl"));
       len4load = PyObject_CallObject(jblen1load, file4args);

       for(expit = mcs5agglengthexpth.begin(); expit != mcs5agglengthexpth.end(); expit++)
       {
         if(expit->first == 550)
         {
           testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len1load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
           char* mcs5sa550 = PyString_AsString(testobj);
           expected_th =  atof(mcs5sa550);
           //std::cout<<"Expected Th:"<<expected_th<<"\n";
           expit->second = expected_th;
         }

         if(expit->first == 1024)
         {
           testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len2load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
           char* mcs5sa1024 = PyString_AsString(testobj);
           expected_th =  atof(mcs5sa1024);
           //std::cout<<"Expected Th:"<<expected_th<<"\n";
           expit->second = expected_th;
         }

         if(expit->first == 2048)
         {
           testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len3load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
           char* mcs5sa2048 = PyString_AsString(testobj);
           expected_th =  atof(mcs5sa2048);
           //std::cout<<"Expected Th:"<<expected_th<<"\n";
           expit->second = expected_th;
         }

         if(expit->first == 3839)
         {
           testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len4load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
           char* mcs5sa3839 = PyString_AsString(testobj);
           expected_th =  atof(mcs5sa3839);
           //std::cout<<"Expected Th:"<<expected_th<<"\n";
           expit->second = expected_th;
         }
       }
       genagglengthexpth = mcs5agglengthexpth;
    }
    if (mcsvalue == 6)
    {
       file1args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFplatform/mcs6_550.pkl"));
       len1load = PyObject_CallObject(jblen1load, file1args);
       file2args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFplatform/mcs6_1024.pkl"));
       len2load = PyObject_CallObject(jblen1load, file2args);
       file3args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFplatform/mcs6_2048.pkl"));
       len3load = PyObject_CallObject(jblen1load, file3args);
       file4args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFplatform/mcs6_3839.pkl"));
       len4load = PyObject_CallObject(jblen1load, file4args);

       for(expit = mcs6agglengthexpth.begin(); expit != mcs6agglengthexpth.end(); expit++)
       {
         if(expit->first == 550)
         {
           testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len1load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
           char* mcs6sa550 = PyString_AsString(testobj);
           expected_th =  atof(mcs6sa550);
           //std::cout<<"Expected Th:"<<expected_th<<"\n";
           expit->second = expected_th;
         }

         if(expit->first == 1024)
         {
           testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len2load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
           char* mcs6sa1024 = PyString_AsString(testobj);
           expected_th =  atof(mcs6sa1024);
           //std::cout<<"Expected Th:"<<expected_th<<"\n";
           expit->second = expected_th;
         }

         if(expit->first == 2048)
         {
           testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len3load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
           char* mcs6sa2048 = PyString_AsString(testobj);
           expected_th =  atof(mcs6sa2048);
           //std::cout<<"Expected Th:"<<expected_th<<"\n";
           expit->second = expected_th;
         }

         if(expit->first == 3839)
         {
           testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len4load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
           char* mcs6sa3839 = PyString_AsString(testobj);
           expected_th =  atof(mcs6sa3839);
           //std::cout<<"Expected Th:"<<expected_th<<"\n";
           expit->second = expected_th;
         }
       }
       genagglengthexpth = mcs6agglengthexpth;
    }
    if (mcsvalue == 7)
    {
       file1args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFplatform/mcs7_550.pkl"));
       len1load = PyObject_CallObject(jblen1load, file1args);
       file2args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFplatform/mcs7_1024.pkl"));
       len2load = PyObject_CallObject(jblen1load, file2args);
       file3args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFplatform/mcs7_2048.pkl"));
       len3load = PyObject_CallObject(jblen1load, file3args);
       file4args = PyTuple_Pack(1, PyString_FromString((char*)"/home/antfbk/NS3/repos/ns-3-allinone/ns-3-29-d977712d48a0/RFplatform/mcs7_3839.pkl"));
       len4load = PyObject_CallObject(jblen1load, file4args);

       for(expit = mcs7agglengthexpth.begin(); expit != mcs7agglengthexpth.end(); expit++)
       {
         if(expit->first == 550)
         {
           testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len1load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
           char* mcs7sa550 = PyString_AsString(testobj);
           expected_th =  atof(mcs7sa550);
           //std::cout<<"Expected Th:"<<expected_th<<"\n";
           expit->second = expected_th;
         }

         if(expit->first == 1024)
         {
           testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len2load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
           char* mcs7sa1024 = PyString_AsString(testobj);
           expected_th =  atof(mcs7sa1024);
           //std::cout<<"Expected Th:"<<expected_th<<"\n";
           expit->second = expected_th;
         }

         if(expit->first == 2048)
         {
           testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len3load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
           char* mcs7sa2048 = PyString_AsString(testobj);
           expected_th =  atof(mcs7sa2048);
           //std::cout<<"Expected Th:"<<expected_th<<"\n";
           expit->second = expected_th;
         }

         if(expit->first == 3839)
         {
           testobj = PyObject_Repr(PyObject_GetItem(PyObject_CallObject(PyObject_GetAttrString(len4load, (char*)"predict"),PyTuple_Pack(1,xtest)), PyLong_FromLong(0)));
           char* mcs7sa3839 = PyString_AsString(testobj);
           expected_th =  atof(mcs7sa3839);
           //std::cout<<"Expected Th 3839:"<<expected_th<<"\n";
           expit->second = expected_th;
         }
       }
       genagglengthexpth = mcs7agglengthexpth;
    }

    uint last_counter=0;
    float highestexp = 0;
    int highexpagglength = 0;
    std::map<int, float>::iterator genit;
    for(genit = genagglengthexpth.begin(); genit != genagglengthexpth.end(); genit++)
    {
      if (highestexp < genit->second)
      {
        highestexp = genit->second;
        //std::cout<<"HIGHEST EXP_TH:"<<highestexp<<"\n";
        highexpagglength = genit->first;
      }

    }

    // previousaggregationlengthperstation is used
    previousaggregationlengthperstation = MsduAggregator::getamsdusizeperstation();
    if (!previous_statistics[stadd])
    {
        previous_statistics[stadd] = (historical_bytes[stadd]-previous_historical_bytes[stadd])/(1024*1024);
    }
    else
    {
      second_statistics[stadd] = previous_statistics[stadd];
      previous_statistics[stadd] = (historical_bytes[stadd]-previous_historical_bytes[stadd])/(1024*1024);

      if (previousaggregationlengthperstation[stadd] != highexpagglength)
      {
        last_counter = 0;
      }
      else
      {
        last_counter+=1;
      }
      if ((second_statistics[stadd] - previous_statistics[stadd])/second_statistics[stadd] < -0.15 && previousaggregationlengthperstation[stadd] != highexpagglength)
      {

        if(bannedagglengthcounterperstation[stadd][agglengthtoindexmap[previousaggregationlengthperstation[stadd]]] == 0)
        {
          std::cout<<"\n";
          bannedagglengthcounterperstation[stadd][agglengthtoindexmap[previousaggregationlengthperstation[stadd]]] = 10;

        }
        else
        {
          bannedagglengthcounterperstation[stadd][agglengthtoindexmap[previousaggregationlengthperstation[stadd]]] += 10;
        }
          //.. check if banned has the length
        //else{}
      }

      if((previous_statistics[stadd] - second_statistics[stadd])/previous_statistics[stadd] < -0.15 && previousaggregationlengthperstation[stadd] != highexpagglength)
      {
        if(bannedagglengthcounterperstation[stadd][agglengthtoindexmap[highexpagglength]] == 0)
        {
          std::cout<<"\n";
          bannedagglengthcounterperstation[stadd][agglengthtoindexmap[highexpagglength]] = 10;

        }
        else
        {
          bannedagglengthcounterperstation[stadd][agglengthtoindexmap[highexpagglength]] += 10;
        }
      }
    }


    uint sm_counter = 0;
    std::map<int, float>::iterator afterbanit;
    for(afterbanit = genagglengthexpth.begin(); afterbanit != genagglengthexpth.end(); afterbanit++)
    {
      sm_counter = bannedagglengthcounterperstation[stadd][agglengthtoindexmap[afterbanit->first]];
      if (sm_counter != 0)
      {
        afterbanit->second = (afterbanit->second)-((afterbanit->second)*sm_counter/100);
      }

      if(bannedagglengthcounterperstation[stadd][agglengthtoindexmap[highexpagglength]] > 0 || last_counter >=5)
      {
        bannedagglengthcounterperstation[stadd][agglengthtoindexmap[highexpagglength]]--;

      }
      else
      {
        if (highestexp < afterbanit->second)
        {
          highestexp = afterbanit->second;
          highexpagglength = afterbanit->first;
          if (previousaggregationlengthperstation[stadd] != highexpagglength)
          {
            last_counter = 0;
          }
        }
      }
    }




    Py_DECREF(len1load);
    Py_DECREF(len2load);
    Py_DECREF(len3load);
    Py_DECREF(len4load);
    Py_DECREF(file1args);
    Py_DECREF(file2args);
    Py_DECREF(file3args);
    Py_DECREF(file4args);
    Py_DECREF(testobj);
    //PyList_SET_ITEM(listObj, 2, PyFloat_FromDouble(double(mcsva)));
    /*PyRun_SimpleString("from time import time,ctime\n"
                       "print 'Today is',ctime(time())\n");*/
    //Py_XDECREF(listObj);

    Py_DECREF(xdata);
    Py_DECREF(xdatatup);
    //Py_DECREF(objectsRepresentation);
    Py_DECREF(xfeatures);
    Py_DECREF(pModulepd);
    Py_DECREF(pFuncpd);
    Py_DECREF(args);
    Py_DECREF(keywords);
    Py_DECREF(xtest);
    Py_DECREF(jblen1load);
    //std::cout<<"HIGHEST EXP_TH:"<<highestexp<<"\n";
    //std::cout<<"The expected throughput is "<<highestexp<<" for aggregation length:"<<highexpagglength<<"\n";
    return std::make_tuple(highexpagglength,highestexp);
  }
    //Py_Finalize();// this would render the interpreter uesless! hence sigsegv
    return std::make_tuple(0,0);
  }

  //compare last and current mcs rfr platform
  int MinstrelHtWifiManager::comparelastandcurrentrfrplatform (float succratio, float gcu, float lastattemptbytes, float minstrel_th, uint16_t currmcs, uint16_t lastmcs, Mac48Address stadd)
    {
      float difference;
      float agglength = 0;

      //only the exp_th values
      difference = (std::get<1>(rfrmodelplatform(succratio, gcu, lastattemptbytes, minstrel_th, currmcs, stadd)) - std::get<1>(rfrmodelplatform(succratio, gcu, lastattemptbytes, minstrel_th, lastmcs, stadd)))/std::get<1>(rfrmodelplatform(succratio, gcu, lastattemptbytes, minstrel_th, lastmcs, stadd));
      std::cout<<"At the station MAC:"<<stadd<<" the current expected throughput is "<<std::get<1>(rfrmodelplatform(succratio, gcu, lastattemptbytes, minstrel_th, currmcs, stadd))<<" the last expected throughput is "<<std::get<1>(rfrmodelplatform(succratio, gcu, lastattemptbytes, minstrel_th, lastmcs, stadd))<< "\n";
      if (difference < 0.05)
      {
        //derive agg length
        //std::cout<<"ad";
        agglength = std::get<0>(rfrmodelplatform(succratio, gcu, lastattemptbytes, minstrel_th, lastmcs, stadd));
      }
      else
      {
        //std::cout<<"ad";
        agglength = std::get<0>(rfrmodelplatform(succratio, gcu, lastattemptbytes, minstrel_th, currmcs, stadd));
      }


      return agglength;
    }

} // namespace ns3
