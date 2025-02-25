/*
 * Copyright (c) 2015-2020 IMDEA Networks Institute
 * Author: Hany Assasa <hany.assasa@gmail.com>
 */
// #include <vector>
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/point-to-point-module.h"
#include "common-functions.h"
#include <iomanip>


NS_LOG_COMPONENT_DEFINE ("BeamformingCBAP");

using namespace ns3;
using namespace std;

//HCC settings

double vel = 1.0;

bool RT_enable = false;




Ptr<DmgApWifiMac> apWifiMac_left;
Ptr<DmgApWifiMac> apWifiMac_right;


std::pair<ApplicationContainer, ApplicationContainer> left_apps;
std::pair<ApplicationContainer, ApplicationContainer> right_apps;
/* Flow monitor */
Ptr<FlowMonitor> monitor;

/*** Access Point Variables ***/
uint8_t stationsTrained = 0;              /* Number of BF trained stations */

/*** Beamforming Service Periods ***/
uint8_t beamformedLinks = 0;              /* Number of beamformed links */

void print_current_time(Ptr<Node> node)
{
    if (node == 0)
    {
        std::cout << "Error: Node is null" << std::endl;
        return;
    }

    Ptr<ConstantVelocityMobilityModel> mobility = node->GetObject<ConstantVelocityMobilityModel>();
    if (mobility != 0)
    {
        Vector pos = mobility->GetPosition();
        std::cout << "Time=" << Simulator::Now().GetSeconds() 
                 << "s, STA0 position: (" << pos.x << "," << pos.y << "," << pos.z << ")" 
                 << std::endl;
        Simulator::Schedule(Seconds(1), &print_current_time, node);
    }
    else
    {
        std::cout << "Error: Could not get mobility model for node" << std::endl;
    }
}

double
CalculateAngle(Ptr<Node> staNode, Ptr<Node> apNode)
{
  Ptr<ConstantVelocityMobilityModel> sta_mobility = staNode->GetObject<ConstantVelocityMobilityModel>();
  Vector sta_pos = sta_mobility->GetPosition();
  Ptr<ConstantVelocityMobilityModel> ap_mobility = apNode->GetObject<ConstantVelocityMobilityModel>();
  Vector ap_pos = ap_mobility->GetPosition();
  double angle = atan2(sta_pos.y - ap_pos.y, sta_pos.x - ap_pos.x);
  angle = angle * 180 / M_PI;
  return angle;
}

void 
CircleMoving(Ptr<Node> apNode, Ptr<Node> staNode, double vel)
{
  Ptr<ConstantVelocityMobilityModel> sta_mobility = staNode->GetObject<ConstantVelocityMobilityModel>();
  Ptr<ConstantVelocityMobilityModel> ap_mobility = apNode->GetObject<ConstantVelocityMobilityModel>();
  Vector sta_post_position = sta_mobility->GetPosition();
  Vector ap_post_position = ap_mobility->GetPosition();
  double rad = pow(sta_post_position.x-ap_post_position.x,2) + pow(sta_post_position.y-ap_post_position.y,2);
  rad = sqrt(rad);
  cout << "rad = " << rad << endl;
  double post_angle = CalculateAngle(staNode, apNode);
  cout << "post_angle = " << post_angle << endl;
  post_angle = post_angle*(M_PI/180);
  double delta_theta = (vel/10)/rad;
  cout << "delta_theta = " << delta_theta*180/M_PI << endl;
  delta_theta = post_angle + delta_theta;
  cout << "new angle = " <<delta_theta*180/M_PI<<endl;
  double x_pos = rad*cos (delta_theta);
  double y_pos = rad*sin (delta_theta);
  cout << "x_pos = " << x_pos << ", y_pos = " << y_pos << endl;
  sta_post_position.x = x_pos;
  sta_post_position.y = y_pos;
  sta_mobility->SetPosition(sta_post_position);
  Simulator::Schedule (MilliSeconds (100), &CircleMoving, apNode, staNode, vel);
}


//HCC function: perform STA1 the TXSS TXOP 



std::pair<ApplicationContainer, ApplicationContainer>
new_InstallPacketSink(string dataRate,Ptr<Node> apNode ,NodeContainer STA_etherNodes, Ipv4InterfaceContainer STA_ethInterface, string socketType, double simulationTime)
{
  
  static int port = 9000;
  cout << "port = " << port << endl;
  ApplicationContainer sinkApps; 
  ApplicationContainer srcApps;

  PacketSinkHelper sinkHelper (socketType, InetSocketAddress (Ipv4Address::GetAny (), port));
  for (uint32_t i = 0; i < STA_etherNodes.GetN(); i++) {
    sinkApps.Add (sinkHelper.Install (STA_etherNodes.Get(i))); // Stas Node 是從 1 開始, 為了跳過代表 AP 的 0.
    sinkApps.Start (Seconds (0.0));
    sinkApps.Stop (Seconds (simulationTime));
  }
  
  for (uint32_t i = 0; i < STA_etherNodes.GetN(); i++) {
    OnOffHelper src (socketType, InetSocketAddress (STA_ethInterface.GetAddress (i), port));
    src.SetAttribute ("MaxPackets", UintegerValue (0));
    src.SetAttribute ("PacketSize", UintegerValue (1448));
    src.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1e6]"));
    src.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    src.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
    srcApps.Add (src.Install (apNode));
    srcApps.Start (Seconds (2.0));
    srcApps.Stop (Seconds (simulationTime));
  }
  std::pair<ApplicationContainer, ApplicationContainer> apps = std::make_pair(sinkApps, srcApps);
  port++;
  return apps;
}

/*要有 node 計算角度, 要有 apps 計算 throughput, 要有 mac interface 計算 snr*/
void
left_new_CalculateThroughput (string file_dir, Ptr<Node> apNode, NodeContainer STA_etherNodes, std::pair<ApplicationContainer, ApplicationContainer> apps ,std::vector<Ptr<DmgStaWifiMac>> staWifiMac, bool left)
{
  int ue_num = staWifiMac.size();
  static std::vector<double> thr(ue_num, 0);
  static std::vector<uint64_t> totalRx(ue_num, 0);
  static std::vector<double> throughput(ue_num, 0);
  static std::vector<double> max_snr(ue_num, 0);
  string left_or_right = left ? "left" : "right";
  for (int i = 0; i < ue_num; i++) {
    string file_name;
    string snrFileName;
    ApplicationContainer temp_appcon = apps.first.Get(i);
    Ptr<PacketSink> temp_sink = StaticCast<PacketSink> (temp_appcon.Get(0));
    
    file_name = file_dir + "STA_" + to_string(i) + "_" + left_or_right + ".csv";
    snrFileName = file_dir + "STA_" + to_string(i) + "_" + left_or_right + "_snr.txt";

    ofstream file(file_name, ios::app);
    Ptr<Node> staNode = STA_etherNodes.Get (i);
    thr[i] = CalculateSingleStreamThroughput (temp_sink, totalRx[i], throughput[i]);
    std::pair<double, uint16_t> max_snr_sector = staWifiMac[i]->HCC_PrintSnrTable(snrFileName);
    max_snr[i] = max_snr_sector.first;
    // if (max_snr[i] < 6 && RT_enable==true && thr[i]==0) staWifiMac [i]->Perform_TXSS_TXOP (apWifiMac->GetAddress ());
    if (max_snr[i] < 6 && RT_enable==true) staWifiMac [i]->Perform_TXSS_TXOP (apWifiMac_left->GetAddress ());
    uint16_t max_sector = max_snr_sector.second;
    double angle = CalculateAngle(staNode, apNode); 
    file << Simulator::Now ().GetSeconds () << "," << thr[i] << "," << max_sector << "," << max_snr[i] << "," << angle << endl;
    file.close();
  }

  string allThrFileName = file_dir + left_or_right + "_all_thr.csv";
  ofstream allThrFile(allThrFileName, ios::app);
  allThrFile << Simulator::Now ().GetSeconds ();
  double total_stas_thr =0;
  for (int i = 0; i < ue_num; i++) {
    if (i != 0) {
      total_stas_thr += thr[i];
    }
    allThrFile << "," << thr[i]<<","<<max_snr[i];
  }
  allThrFile << "," << total_stas_thr << std::endl;
  allThrFile.close();
  Simulator::Schedule (MilliSeconds (100), &left_new_CalculateThroughput, file_dir, apNode, STA_etherNodes, apps, staWifiMac, left);
}

void
right_new_CalculateThroughput (string file_dir, Ptr<Node> apNode, NodeContainer STA_etherNodes, std::pair<ApplicationContainer, ApplicationContainer> apps ,std::vector<Ptr<DmgStaWifiMac>> staWifiMac, bool left)
{
  int ue_num = staWifiMac.size();
  static std::vector<double> thr(ue_num, 0);
  static std::vector<uint64_t> totalRx(ue_num, 0);
  static std::vector<double> throughput(ue_num, 0);
  static std::vector<double> max_snr(ue_num, 0);
  string left_or_right = left ? "left" : "right";
  for (int i = 0; i < ue_num; i++) {
    string file_name;
    string snrFileName;
    ApplicationContainer temp_appcon = apps.first.Get(i);
    Ptr<PacketSink> temp_sink = StaticCast<PacketSink> (temp_appcon.Get(0));
    
    file_name = file_dir + "STA_" + to_string(i) + "_" + left_or_right + ".csv";
    snrFileName = file_dir + "STA_" + to_string(i) + "_" + left_or_right + "_snr.txt";

    ofstream file(file_name, ios::app);
    Ptr<Node> staNode = STA_etherNodes.Get (i);
    thr[i] = CalculateSingleStreamThroughput (temp_sink, totalRx[i], throughput[i]);
    std::pair<double, uint16_t> max_snr_sector = staWifiMac[i]->HCC_PrintSnrTable(snrFileName);
    max_snr[i] = max_snr_sector.first;
    // if (max_snr[i] < 6 && RT_enable==true && thr[i]==0) staWifiMac [i]->Perform_TXSS_TXOP (apWifiMac->GetAddress ());
    if (max_snr[i] < 6 && RT_enable==true) staWifiMac [i]->Perform_TXSS_TXOP (apWifiMac_right->GetAddress ());
    uint16_t max_sector = max_snr_sector.second;
    double angle = CalculateAngle(staNode, apNode); 
    file << Simulator::Now ().GetSeconds () << "," << thr[i] << "," << max_sector << "," << max_snr[i] << "," << angle << endl;
    file.close();
  }

  string allThrFileName = file_dir + left_or_right + "_all_thr.csv";
  ofstream allThrFile(allThrFileName, ios::app);
  allThrFile << Simulator::Now ().GetSeconds ();
  double total_stas_thr =0;
  for (int i = 0; i < ue_num; i++) {
    if (i != 0) {
      total_stas_thr += thr[i];
    }
    allThrFile << "," << thr[i]<<","<<max_snr[i];
  }
  allThrFile << "," << total_stas_thr << std::endl;
  allThrFile.close();
  Simulator::Schedule (MilliSeconds (100), &right_new_CalculateThroughput, file_dir, apNode, STA_etherNodes, apps, staWifiMac, left);
}

void 
new_CreateFile (string file_dir, int ue_num)
{
  cout << "Creating file" << endl;
  for (int i = 0; i < ue_num; i++) {
    string left_file_name;
    string right_file_name;
    string left_snrFileName;
    string right_snrFileName;
    left_file_name    = file_dir + "STA_" + to_string(i) + "_left.csv";
    right_file_name   = file_dir + "STA_" + to_string(i) + "_right.csv";
    left_snrFileName  = file_dir + "STA_" + to_string(i) + "_left_snr.txt";
    right_snrFileName = file_dir + "STA_" + to_string(i) + "_right_snr.txt";
    ofstream left_file (left_file_name, ios::out);
    ofstream right_file (right_file_name, ios::out);
    ofstream left_snrFile (left_snrFileName, ios::out);
    ofstream right_snrFile (right_snrFileName, ios::out);
    left_file << "Time [s],Throughput [Mbps],Max SectorID,Max SNR[dB],Angle[deg]" << std::endl;
    left_file.close();
    right_file << "Time [s],Throughput [Mbps],Max SectorID,Max SNR[dB],Angle[deg]" << std::endl;
    right_file.close();
    left_snrFile << "Time [s],Max SNR[dB],SectorID" << std::endl;
    left_snrFile.close();
    right_snrFile << "Time [s],Max SNR[dB],SectorID" << std::endl;
    right_snrFile.close();
  }
  string left_allThrFileName = file_dir + "left_all_thr.csv";
  string right_allThrFileName = file_dir + "right_all_thr.csv";
  ofstream left_allThrFile(left_allThrFileName, ios::out);
  ofstream right_allThrFile(right_allThrFileName, ios::out);
  left_allThrFile << "Time [s],";
  right_allThrFile << "Time [s],";
  for (int i = 0; i < ue_num; i++) {
    left_allThrFile << "STA_" << i << "_Thr[Mbps]" << ",";
    right_allThrFile << "STA_" << i << "_Thr[Mbps]" << ",";
    left_allThrFile << "STA_" << i << "_MaxSNR[dB]" << ",";
    right_allThrFile << "STA_" << i << "_MaxSNR[dB]" << ",";
  }
  left_allThrFile << "Non_Moving_Thr[Mbps]" << std::endl;
  left_allThrFile.close();
  right_allThrFile << "Non_Moving_Thr[Mbps]" << std::endl;
  right_allThrFile.close();
}


void
StationAssoicated (Ptr<DmgStaWifiMac> staWifiMac, Mac48Address address, uint16_t aid)
{
  std::cout <<"Time @ " << Simulator::Now ().GetSeconds () << " DMG STA " << staWifiMac->GetAddress () << " associated with DMG AP " << address << std::endl;
  std::cout << "Association ID (AID) = " << aid << std::endl;
  if (address == apWifiMac_left->GetAddress ()) {
    staWifiMac->Perform_TXSS_TXOP (apWifiMac_left->GetAddress ());
  } 
  else if (address == apWifiMac_right->GetAddress ()) {
    staWifiMac->Perform_TXSS_TXOP (apWifiMac_right->GetAddress ());
  }
  else {
    std::cout << "Error: Unknown AP address" << std::endl;
  }
}

void
SLSCompleted (Ptr<DmgWifiMac> wifiMac, SlsCompletionAttrbitutes attributes)
{
  if (attributes.accessPeriod == CHANNEL_ACCESS_BHI)
    {
      if (wifiMac == apWifiMac_left)
        {
          std::cout << "DMG AP Left" << apWifiMac_left->GetAddress () <<
                       " completed SLS phase with DMG STA " << attributes.peerStation << std::endl;
        }
      else if (wifiMac == apWifiMac_right)
        {
          std::cout << "DMG AP Right" << apWifiMac_right->GetAddress () <<
                       " completed SLS phase with DMG STA " << attributes.peerStation << std::endl;
        }
      else
        {
          std::cout << "DMG STA " << wifiMac->GetAddress ()
                    << " completed SLS phase with DMG AP " << attributes.peerStation << std::endl;
        }
      std::cout << "Best Tx Antenna Configuration: AntennaID=" << uint16_t (attributes.antennaID)
                << ", SectorID=" << uint16_t (attributes.sectorID) << std::endl;
    }
  else if (attributes.accessPeriod == CHANNEL_ACCESS_DTI)
    {
      beamformedLinks++;
      std::cout << "DMG STA " << wifiMac->GetAddress () << " completed SLS phase with DMG STA " << attributes.peerStation << std::endl;
      std::cout << "The best antenna configuration is AntennaID=" << uint16_t (attributes.antennaID)
                << ", SectorID=" << uint16_t (attributes.sectorID) << std::endl;
      // if (beamformedLinks == 2)
      //   {
      //     apWifiMac->PrintSnrTable ();
      //     staWifiMac->PrintSnrTable ();
      //   }
    }
}

void
ActiveTxSectorIDChanged (Ptr<DmgWifiMac> wifiMac, SectorID oldSectorID, SectorID newSectorID)
{
  std::cout << "DMG STA: " << wifiMac->GetAddress () << " , SectorID=" << uint16_t (newSectorID) << std::endl;
}

void 
position_seeker (Ptr<Node> node)
{
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
  std::cout << "position of node: " << mobility->GetPosition () << std::endl;
  Simulator::Schedule (Seconds (1), &position_seeker, node);
}

void PrintInterfaceInfo(Ptr<Node> node) {
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    std::cout << "Node " << node->GetId() << " interfaces:" << std::endl;
    
    for (uint32_t i = 0; i < ipv4->GetNInterfaces(); i++) {
        for (uint32_t j = 0; j < ipv4->GetNAddresses(i); j++) {
            Ipv4InterfaceAddress addr = ipv4->GetAddress(i, j);
            std::cout << "Interface " << i 
                      << ": addr=" << addr.GetLocal()
                      << " mask=" << addr.GetMask()
                      << std::endl;
        }
    }
    std::cout << "------------------------" << std::endl;
}



int
main (int argc, char *argv[])
{

  LogComponentEnable ("GlobalRouter", LOG_LEVEL_ALL);
  LogComponentEnable ("Ipv4GlobalRouting", LOG_LEVEL_ALL);
  //string applicationType = "bulk";              /* Type of the Tx application */
  bool activateApp = true;                      /* Flag to indicate whether we activate onoff or bulk App */
  string socketType = "ns3::TcpSocketFactory";  /* Socket Type (TCP/UDP) */
  uint32_t packetSize = 1448;                   /* Application payload size in bytes. */
  string dataRate = "300Mbps";                  /* Application data rate. */
  // string tcpVariant = "NewReno";                /* TCP Variant Type. */
  string tcpVariant = "Bic";                /* TCP Variant Type. */
  uint32_t bufferSize = 131072;                 /* TCP Send/Receive Buffer Size. */
  uint32_t maxPackets = 0;                      /* Maximum Number of Packets */
  string msduAggSize = "max";                     /* The maximum aggregation size for A-MSDU in Bytes. */
  string mpduAggSize = "max";                  /* The maximum aggregation size for A-MSPU in Bytes. */
  string queueSize = "4000p";                   /* Wifi MAC Queue Size. */
  string phyMode = "EDMG_OFDM_MCS8";                 /* Type of the Physical Layer. */
  bool verbose = false;                         /* Print Logging Information. */
  double simulationTime = 10;                   /* Simulation time in seconds. */
  bool pcapTracing = true;                     /* PCAP Tracing is enabled or not. */

  /* Command line argument parser setup. */
  CommandLine cmd;
  //HCC parameters
  //--------------------------------
  string file_dir;
  int user_num = 2;
  string comment;
  bool circle = false;
  //--------------------------------
  cmd.AddValue ("ue", "The number of STAs", user_num);
  cmd.AddValue ("fileDir", "The directory to store the throughput files", file_dir);
  cmd.AddValue ("comment", "The comment of the experiment", comment);
  cmd.AddValue ("activateApp", "Whether to activate data transmission or not", activateApp);
  cmd.AddValue ("vel", "The velocity of STA0", vel);
  cmd.AddValue ("retrain","Whether to retrain the beamforming or not", RT_enable);
  cmd.AddValue ("circle", "Set the moving path as circle", circle);
  //cmd.AddValue ("applicationType", "Type of the Tx Application: onoff or bulk", applicationType);
  cmd.AddValue ("packetSize", "Application packet size in bytes", packetSize);
  cmd.AddValue ("dataRate", "Application data rate", dataRate);
  cmd.AddValue ("maxPackets", "Maximum number of packets to send", maxPackets);
  cmd.AddValue ("tcpVariant", TCP_VARIANTS_NAMES, tcpVariant);
  cmd.AddValue ("socketType", "Type of the Socket (ns3::TcpSocketFactory, ns3::UdpSocketFactory)", socketType);
  cmd.AddValue ("bufferSize", "TCP Buffer Size (Send/Receive) in Bytes", bufferSize);
  cmd.AddValue ("msduAggSize", "The maximum aggregation size for A-MSDU in Bytes", msduAggSize);
  cmd.AddValue ("mpduAggSize", "The maximum aggregation size for A-MPDU in Bytes", mpduAggSize);
  cmd.AddValue ("verbose", "Turn on all WifiNetDevice log components", verbose);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("pcap", "Enable PCAP Tracing", pcapTracing);
  cmd.Parse (argc, argv);

  /* Validate A-MSDU and A-MPDU values */
  ValidateFrameAggregationAttributes (msduAggSize, mpduAggSize);
  /* Configure RTS/CTS and Fragmentation */
  ConfigureRtsCtsAndFragmenatation (false,0);
  /* Wifi MAC Queue Parameters */
  ChangeQueueSize (queueSize);

  /*** Configure TCP Options ***/
  ConfigureTcpOptions (tcpVariant, packetSize, bufferSize);

  /**** DmgWifiHelper is a meta-helper ****/
  DmgWifiHelper wifi;

  /* Basic setup */
  wifi.SetStandard (WIFI_PHY_STANDARD_80211ay);

  /* Turn on logging */
  if (verbose)
    {
      wifi.EnableLogComponents ();
      LogComponentEnable ("BeamformingCBAP", LOG_LEVEL_ALL);
    }

  /**** Set up Channel ****/
  DmgWifiChannelHelper wifiChannel ;
  /* Simple propagation delay model */
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  /* Friis model with standard-specific wavelength */
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel", "Frequency", DoubleValue (60.48e9));

  /**** Setup physical layer ****/
  DmgWifiPhyHelper wifiPhy = DmgWifiPhyHelper::Default ();
  /* Nodes will be added to the channel we set up earlier */
  wifiPhy.SetChannel (wifiChannel.Create ());
  /* All nodes transmit at 10 dBm == 10 mW, no adaptation */
  wifiPhy.Set ("TxPowerStart", DoubleValue (10.0));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (10.0));
  wifiPhy.Set ("TxPowerLevels", UintegerValue (1));
  /* Set operating channel */
  wifiPhy.Set ("ChannelNumber", UintegerValue (2));
  wifiPhy.Set ("SupportOfdmPhy", BooleanValue (true));
  /* Set default algorithm for all nodes to be constant rate */
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue (phyMode));
  wifiPhy.SetErrorRateModel ("ns3::DmgErrorModel", "FileName", StringValue ("WigigFiles/ErrorModel/LookupTable_1458.txt"));
  /* Make four nodes and set them up with the phy and the mac */
  
  // Create two AP nodes
  NodeContainer AP_Nodes;
  AP_Nodes.Create (2);
  Ptr<Node> leftAp = AP_Nodes.Get (0);
  Ptr<Node> rightAp = AP_Nodes.Get (1);
 
  // Create STAs mmWave nodes
  NodeContainer STA_mmWaveNodes;
  STA_mmWaveNodes.Create (2*user_num);

  // merge all mmWave nodes
  NodeContainer allmmWaveNodes;
  allmmWaveNodes.Add (AP_Nodes);
  allmmWaveNodes.Add (STA_mmWaveNodes);

  /* Add a DMG upper mac */
  DmgWifiMacHelper STA_LeftWifiMac = DmgWifiMacHelper::Default ();
  DmgWifiMacHelper STA_RightWifiMac = DmgWifiMacHelper::Default ();
  DmgWifiMacHelper AP_LeftWifiMac = DmgWifiMacHelper::Default ();
  DmgWifiMacHelper AP_RightWifiMac = DmgWifiMacHelper::Default ();

  /* Install DMG PCP/AP Node */
  Ssid leftSsid = Ssid ("Left");
  Ssid rightSsid = Ssid ("Right");
  Ssid failed = Ssid ("Failed");

  AP_LeftWifiMac.SetType ("ns3::DmgApWifiMac",
                        "Ssid", SsidValue(leftSsid),
                        // "Ssid", SsidValue(failed),  
                        "BE_MaxAmpduSize", StringValue (mpduAggSize),
                        "BE_MaxAmsduSize", StringValue (msduAggSize),
                        "SSSlotsPerABFT", UintegerValue (8), "SSFramesPerSlot", UintegerValue (8),
                        "EDMGSupported", BooleanValue (true),
                        "BeaconInterval", TimeValue (MicroSeconds (102400)));

  AP_RightWifiMac.SetType ("ns3::DmgApWifiMac",
                        "Ssid", SsidValue(rightSsid),
                        // "Ssid", SsidValue(failed),  
                        "BE_MaxAmpduSize", StringValue (mpduAggSize),
                        "BE_MaxAmsduSize", StringValue (msduAggSize),
                        "SSSlotsPerABFT", UintegerValue (8), "SSFramesPerSlot", UintegerValue (8),
                        "EDMGSupported", BooleanValue (true),
                        "BeaconInterval", TimeValue (MicroSeconds (102400)));

  /* Set Analytical Codebook for the DMG Devices */
  wifi.SetCodebook ("ns3::CodebookAnalytical",
                    "CodebookType", EnumValue (SIMPLE_CODEBOOK),
                    "Antennas", UintegerValue (1),
                    "Sectors", UintegerValue (8));

  NetDeviceContainer apDevice;
  apDevice.Add (wifi.Install (wifiPhy, AP_LeftWifiMac, leftAp));//左側的AP
  apDevice.Add (wifi.Install (wifiPhy, AP_RightWifiMac, rightAp));//右側的AP
  cout << "mac address of left AP: " << apDevice.Get (0)->GetAddress () << endl;
  cout << "mac address of right AP: " << apDevice.Get (1)->GetAddress () << endl;

  /* Install DMG STA Nodes */
  STA_LeftWifiMac.SetType ("ns3::DmgStaWifiMac",
                      "Ssid", SsidValue (leftSsid), "ActiveProbing", BooleanValue (false),
                      "EDMGSupported", BooleanValue (true),
                      "BE_MaxAmpduSize", StringValue (mpduAggSize),
                      "BE_MaxAmsduSize", StringValue (msduAggSize));

  STA_RightWifiMac.SetType ("ns3::DmgStaWifiMac",
                      "Ssid", SsidValue (rightSsid), "ActiveProbing", BooleanValue (false),
                      "EDMGSupported", BooleanValue (true),
                      "BE_MaxAmpduSize", StringValue (mpduAggSize),
                      "BE_MaxAmsduSize", StringValue (msduAggSize));


  NetDeviceContainer STA_LeftDevices;
  NetDeviceContainer STA_RightDevices;
  for (int i = 0; i < user_num; i++) {
    
    STA_LeftDevices.Add(wifi.Install (wifiPhy, STA_LeftWifiMac, STA_mmWaveNodes.Get (2*i)));//偶數為 STAs 與 左側 AP 連接的 netdevice
    STA_RightDevices.Add(wifi.Install (wifiPhy, STA_RightWifiMac, STA_mmWaveNodes.Get (2*i+1)));//奇數為 STAs 與 右側 AP 連接的 netdevice
  }

  //代表 STA 實際上的節點
  NodeContainer STA_ethNodes;
  STA_ethNodes.Create (user_num);
  
  // 儲存 2*user_num 個節點，代表每個 mmWave interface 和 STA 實體節點的 p2p 連結
  std::vector<NodeContainer> STA_NodePair;
  for (int i = 0; i < user_num; i++) {
    NodeContainer STA_leftNodePair = NodeContainer(STA_mmWaveNodes.Get(2*i), STA_ethNodes.Get(i));
    NodeContainer STA_rightNodePair = NodeContainer(STA_mmWaveNodes.Get(2*i+1), STA_ethNodes.Get(i));
    STA_NodePair.push_back(STA_leftNodePair);
    STA_NodePair.push_back(STA_rightNodePair);
  }
  cout << "STA_NodePair has been created, size: " << STA_NodePair.size() << endl<<  endl;

  // 設定 STAs 內部的 p2p 參數
  PointToPointHelper p2pInSTA_helper;
  p2pInSTA_helper.SetDeviceAttribute ("DataRate", StringValue ("5Gbps"));
  p2pInSTA_helper.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (0.1)));
  p2pInSTA_helper.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("4294967295p"));

  // 安裝 STAs 內部的 p2p 連接
  NetDeviceContainer p2pInSTA_devices_left;
  NetDeviceContainer p2pInSTA_devices_right;

  for (uint32_t i = 0; i < STA_NodePair.size(); i++) {
    if (i % 2 == 0) {
      p2pInSTA_devices_left.Add (p2pInSTA_helper.Install (STA_NodePair[i]));
    }
    else {
      p2pInSTA_devices_right.Add (p2pInSTA_helper.Install (STA_NodePair[i]));
    }
  }
    cout << "p2pInSTA_devices_left has been created" << p2pInSTA_devices_left.GetN() << endl;
    cout << "p2pInSTA_devices_right has been created" << p2pInSTA_devices_right.GetN() << endl;


  /* Setting mobility model */
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (-1.0, 0.0, 5.0));   /* DMG PCP/AP */
  positionAlloc->Add (Vector (1.0, 0.0, 5.0));   /* DMG PCP/AP */
  
  // Position of STA_ethNodes
  for (uint32_t i = 0; i < STA_ethNodes.GetN(); i++) {
    if (i == 0)
      // positionAlloc->Add (Vector (-1.0, -1.0, 0.0));   /* DMG STA0*/
      positionAlloc->Add (Vector (0.0, -1.0, 0.0));   /* DMG STA0*/
    else
      positionAlloc->Add (Vector (0.0, -1.0, 0.0));   /* DMG STA i_th*/
  }
  
  // Position of STA_mmWaveNodes
  for (uint32_t i = 0; i < STA_mmWaveNodes.GetN(); i+=2) {
    if (i == 0){
      // positionAlloc->Add (Vector (-1.0, -1.0, 0.0));   /* DMG STA0*/
      // positionAlloc->Add (Vector (-1.0, -1.0, 0.0));   /* DMG STA0*/
      positionAlloc->Add (Vector (0.0, -1.0, 0.0));   /* DMG STA0*/
      positionAlloc->Add (Vector (0.0, -1.0, 0.0));   /* DMG STA0*/
    }
    else{
      positionAlloc->Add (Vector (0.0, -1.0, 0.0));   /* DMG STA i_th*/
      positionAlloc->Add (Vector (0.0, -1.0, 0.0));   /* DMG STA i_th*/
    }
  }

  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (AP_Nodes); //APs
  mobility.Install (STA_ethNodes); //STAs
  mobility.Install (STA_mmWaveNodes); //mmWave STAs
  
  //STA0's mobility model
  
  // std::vector<Ptr<ConstantVelocityMobilityModel>> sta0_mobility;
  // sta0_mobility.push_back(STA_ethNodes.Get (0)->GetObject<ConstantVelocityMobilityModel>());
  // sta0_mobility.push_back(STA_mmWaveNodes.Get (0)->GetObject<ConstantVelocityMobilityModel>());
  // sta0_mobility.push_back(STA_mmWaveNodes.Get (1)->GetObject<ConstantVelocityMobilityModel>());

  cout << "position of STA0's right mmwave: " << STA_mmWaveNodes.Get (1)->GetObject<ConstantVelocityMobilityModel>()->GetPosition () << endl;
  cout << "position of STA0's left mmwave: " << STA_mmWaveNodes.Get (0)->GetObject<ConstantVelocityMobilityModel>()->GetPosition () << endl;
  cout << "position of STA0's eth: " << STA_ethNodes.Get (0)->GetObject<ConstantVelocityMobilityModel>()->GetPosition () << endl;

  Ptr<ConstantVelocityMobilityModel> ap_left_mobility = AP_Nodes.Get (0)->GetObject<ConstantVelocityMobilityModel>();
  Ptr<ConstantVelocityMobilityModel> ap_right_mobility = AP_Nodes.Get (1)->GetObject<ConstantVelocityMobilityModel>();
  cout << "position of left AP: " << ap_left_mobility->GetPosition () << endl;
  cout << "position of right AP: " << ap_right_mobility->GetPosition () << endl;
  
  
  if (circle)
  {
    Simulator::Schedule (Seconds (3.1), &CircleMoving, leftAp, STA_ethNodes.Get(0), vel);
    Simulator::Schedule (Seconds (3.1), &CircleMoving, leftAp, STA_mmWaveNodes.Get(0), vel);
    Simulator::Schedule (Seconds (3.1), &CircleMoving, leftAp, STA_mmWaveNodes.Get(1), vel);
  }
  // else 
  // {
  //   for (uint32_t i = 0; i < sta0_mobility.size(); i++) {
  //     Simulator::Schedule (Seconds (2.1), &ConstantVelocityMobilityModel::SetVelocity, sta0_mobility[i], Vector(vel, 0.0, 0.0));
  //     Simulator::Schedule (Seconds (10.1), &ConstantVelocityMobilityModel::SetVelocity, sta0_mobility[i], Vector(-vel, 0.0, 0.0));
  //   }
  // }
  
  
  


  // Simulator::Schedule (Seconds (4.1), &ConstantVelocityMobilityModel::SetPosition, sta0_mobility, Vector(-vel, -1.0, 0.0));
  // Simulator::Schedule (Seconds (8.1), &ConstantVelocityMobilityModel::SetPosition, sta0_mobility, Vector(vel, -1.0, 0.0));


  /* Internet stack*/
  InternetStackHelper stack;
  stack.Install (AP_Nodes);
  stack.Install (STA_ethNodes);
  stack.Install (STA_mmWaveNodes);

  Ipv4AddressHelper address;
  Ipv4InterfaceContainer apInterfaces;
  Ipv4InterfaceContainer STA_LeftInterfaces;
  Ipv4InterfaceContainer STA_RightInterfaces;
  Ipv4InterfaceContainer STA_ethInterfaces;

  // 先分配 AP 和 mmWave 的連接
  address.SetBase ("10.1.1.0", "255.255.255.0");
  apInterfaces.Add (address.Assign (apDevice.Get(0)));
  STA_LeftInterfaces = address.Assign (STA_LeftDevices);
  address.SetBase ("10.1.2.0", "255.255.255.0");
  apInterfaces.Add (address.Assign (apDevice.Get(1)));
  STA_RightInterfaces = address.Assign (STA_RightDevices);
  address.SetBase ("10.1.3.0", "255.255.255.0");
  STA_ethInterfaces = address.Assign (p2pInSTA_devices_left);
  STA_ethInterfaces.Add (address.Assign (p2pInSTA_devices_right));
  

  for (uint32_t i = 0; i < STA_LeftInterfaces.GetN(); i++) {
    cout << "STA " << i << " Left mmWave IP: " << STA_LeftInterfaces.GetAddress (i) << endl;
  }
  for (uint32_t i = 0; i < STA_RightInterfaces.GetN(); i++) {
    cout << "STA " << i << " Right mmWave IP: " << STA_RightInterfaces.GetAddress (i) << endl;
  } 
  for (uint32_t i = 0; i < STA_ethInterfaces.GetN(); i++) {
    cout << "STA " << i << " eth IP: " << STA_ethInterfaces.GetAddress (i) << endl;
  }
  cout << "IP has been assigned" << endl;
  
  cout << "NodeList::GetNNodes(): " << NodeList::GetNNodes() << endl;
  // return 0;
  
  // 在 PopulateRoutingTables 之前調用
  std::cout << "\n=== AP Nodes Interfaces ===" << std::endl;
  for (uint32_t i = 0; i < AP_Nodes.GetN(); i++) {
      PrintInterfaceInfo(AP_Nodes.Get(i));
  }

  std::cout << "\n=== mmWave Nodes Interfaces ===" << std::endl;
  for (uint32_t i = 0; i < STA_mmWaveNodes.GetN(); i++) {
      PrintInterfaceInfo(STA_mmWaveNodes.Get(i));
  }

  std::cout << "\n=== UE Nodes Interfaces ===" << std::endl;
  for (uint32_t i = 0; i < STA_ethNodes.GetN(); i++) {
      PrintInterfaceInfo(STA_ethNodes.Get(i));
  }
  

  /* Populate routing table */
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  /* We do not want any ARP packets */
  PopulateArpCache ();


  // return 0; // node ~ IP 相關設定檢查點
  
  if (activateApp)
    {
      /* Install Simple UDP Server on the DMG AP */
      

      left_apps = new_InstallPacketSink(dataRate, leftAp, STA_ethNodes , STA_ethInterfaces, socketType, simulationTime);
      right_apps = new_InstallPacketSink(dataRate, rightAp, STA_ethNodes , STA_ethInterfaces, socketType, simulationTime);
      cout << "Packet sink has been installed" << endl;      
    }
  // return 0; // 安裝 app 相關設定檢查點

  Ptr<PacketSink> left_packetSink[user_num];
  Ptr<PacketSink> right_packetSink[user_num];
  Ptr<OnOffApplication> left_onoff[user_num];
  Ptr<OnOffApplication> right_onoff[user_num];
  for (int i = 0; i < user_num; i++) {
        ApplicationContainer temp_appcon = left_apps.first.Get(i);
        Ptr<PacketSink> temp_sink = StaticCast<PacketSink> (temp_appcon.Get(0));
        ApplicationContainer temp_appcon2 = left_apps.second.Get(i);
        Ptr<OnOffApplication> temp_src = StaticCast<OnOffApplication> (temp_appcon2.Get(0));
        left_packetSink[i] = temp_sink;
        left_onoff[i] = temp_src;
        temp_appcon = right_apps.first.Get(i);
        temp_sink = StaticCast<PacketSink> (temp_appcon.Get(0));
        temp_appcon2 = right_apps.second.Get(i);
        temp_src = StaticCast<OnOffApplication> (temp_appcon2.Get(0));
        right_packetSink[i] = temp_sink;
        right_onoff[i] = temp_src;
       }
  // return 0; // 安裝 app 相關設定檢查點
  /* Enable Traces */
  if (pcapTracing)
    {
      wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
      wifiPhy.EnablePcap ("Traces/AccessPoint_"+comment, apDevice, false);
      wifiPhy.EnablePcap ("Traces/StaNode_"+comment, STA_LeftDevices, false);
      wifiPhy.EnablePcap ("Traces/StaNode_"+comment, STA_RightDevices, false);
    }
  // return 0; // 安裝 pcap 相關設定檢查點
  /* Stations */
  Ptr<WifiNetDevice> apWifiNetDevice_left = StaticCast<WifiNetDevice> (apDevice.Get (0));
  Ptr<WifiNetDevice> apWifiNetDevice_right = StaticCast<WifiNetDevice> (apDevice.Get (1));
  Ptr<WifiNetDevice> staWifiNetDevice_left[user_num];
  Ptr<WifiNetDevice> staWifiNetDevice_right[user_num];
  for (int i = 0; i < user_num; i++) {
    staWifiNetDevice_left[i] = StaticCast<WifiNetDevice> (STA_LeftDevices.Get (i));
    staWifiNetDevice_right[i] = StaticCast<WifiNetDevice> (STA_RightDevices.Get (i));
  }

  apWifiMac_left = StaticCast<DmgApWifiMac> (apWifiNetDevice_left->GetMac ());
  apWifiMac_right = StaticCast<DmgApWifiMac> (apWifiNetDevice_right->GetMac ());
  std::vector<Ptr<DmgStaWifiMac>> staWifiMac_left(user_num);
  std::vector<Ptr<DmgStaWifiMac>> staWifiMac_right(user_num);
  for (int i = 0; i < user_num; i++) {
    staWifiMac_left[i] = StaticCast<DmgStaWifiMac> (staWifiNetDevice_left[i]->GetMac ());
    staWifiMac_right[i] = StaticCast<DmgStaWifiMac> (staWifiNetDevice_right[i]->GetMac ());
  } 
  // return 0; // 取得 MAC 資訊相關設定檢查點
  /** Connect Traces **/
  for (int i = 0; i < user_num; i++) {
    staWifiMac_left[i]->TraceConnectWithoutContext ("Assoc", MakeBoundCallback (&StationAssoicated, staWifiMac_left[i]));
    staWifiMac_right[i]->TraceConnectWithoutContext ("Assoc", MakeBoundCallback (&StationAssoicated, staWifiMac_right[i]));
  }
  // return 0; // Assoc 資訊相關設定檢查點
  // apWifiMac->TraceConnectWithoutContext ("SLSCompleted", MakeBoundCallback (&SLSCompleted, apWifiMac));
  // for (int i = 0; i < user_num; i++) {
  //   staWifiMac[i]->TraceConnectWithoutContext ("SLSCompleted", MakeBoundCallback (&SLSCompleted, staWifiMac[i]));
  // }


  apWifiMac_left->GetCodebook ()->TraceConnectWithoutContext ("ActiveTxSectorID", MakeBoundCallback (&ActiveTxSectorIDChanged, apWifiMac_left));
  apWifiMac_right->GetCodebook ()->TraceConnectWithoutContext ("ActiveTxSectorID", MakeBoundCallback (&ActiveTxSectorIDChanged, apWifiMac_right));

  for (int i = 0; i < user_num; i++) {  
    staWifiMac_left[i]->GetCodebook ()->TraceConnectWithoutContext ("ActiveTxSectorID", MakeBoundCallback (&ActiveTxSectorIDChanged, staWifiMac_left[i]));
    staWifiMac_right[i]->GetCodebook ()->TraceConnectWithoutContext ("ActiveTxSectorID", MakeBoundCallback (&ActiveTxSectorIDChanged, staWifiMac_right[i]));
  }

  FlowMonitorHelper flowmon;
  if (activateApp)
    {
      /* Install FlowMonitor on all nodes */
      monitor = flowmon.Install (allmmWaveNodes);

      /* Schedule Throughput Calulcations */
      string filename = file_dir + "arp_cache_left.txt";
      Simulator::Schedule (Seconds (1.0), &new_CreateFile, file_dir, user_num);
      Simulator::Schedule (Seconds (2.1), &left_new_CalculateThroughput, file_dir,leftAp,STA_ethNodes,left_apps,staWifiMac_left,true);
      Simulator::Schedule (Seconds (2.1), &right_new_CalculateThroughput, file_dir,rightAp,STA_ethNodes,right_apps,staWifiMac_right,false);
    }
  /* Schedule many TXSS CBAPs during the data transmission interval. */
  
  // Simulator::Schedule (Seconds (2.1), &print_current_time, STA_ethNodes.Get (0));
  // Simulator::Schedule (Seconds (2.1), &position_seeker, STA_mmWaveNodes.Get (3));

  Simulator::Stop (Seconds (simulationTime + 0.101));
  Simulator::Run ();
  Simulator::Destroy ();

  if (activateApp)
    {
      PrintFlowMonitorStatistics (flowmon, monitor, simulationTime - 1);

      /* Print Application Layer Results Summary */
      std::cout << "\nApplication Layer Statistics:" << std::endl;
      std::cout << "Left AP" << std::endl;
      for (int i = 0; i < user_num; i++)
        {
          std::cout << "  Tx Packets: " << left_onoff[i]->GetTotalTxPackets () << std::endl;
          std::cout << "  Tx Bytes:   " << left_onoff[i]->GetTotalTxBytes () << std::endl;
        }
      for (int i = 0; i < user_num; i++)
        {
          std::cout << "----------------------------------------" << std::endl;
          std::cout << "User " << i << " Statistics:" << std::endl;
          std::cout << "  Rx Packets: " << left_packetSink[i]->GetTotalReceivedPackets () << std::endl;
          std::cout << "  Rx Bytes:   " << left_packetSink[i]->GetTotalRx () << std::endl;
          std::cout << "  Throughput: " << left_packetSink[i]->GetTotalRx () * 8.0 / ((simulationTime - 2.0) * 1e6) << " Mbps" << std::endl;
          std::cout << "----------------------------------------" << std::endl<<std::endl;
        }

      std::cout << std::endl << "Right AP" << std::endl;
      for (int i = 0; i < user_num; i++)
        {
          std::cout << "  Tx Packets: " << right_onoff[i]->GetTotalTxPackets () << std::endl;
          std::cout << "  Tx Bytes:   " << right_onoff[i]->GetTotalTxBytes () << std::endl;
        }
      for (int i = 0; i < user_num; i++)
        {
          std::cout << "----------------------------------------" << std::endl;
          std::cout << "User " << i << " Statistics:" << std::endl;
          std::cout << "  Rx Packets: " << right_packetSink[i]->GetTotalReceivedPackets () << std::endl;
          std::cout << "  Rx Bytes:   " << right_packetSink[i]->GetTotalRx () << std::endl;
          std::cout << "  Throughput: " << right_packetSink[i]->GetTotalRx () * 8.0 / ((simulationTime - 2.0) * 1e6) << " Mbps" << std::endl;
          std::cout << "----------------------------------------" << std::endl<<std::endl;
        }
    }

  

  return 0;
}
