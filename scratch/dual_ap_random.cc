/*
 * Copyright (c) 2015-2020 IMDEA Networks Institute
 * Author: Hany Assasa <hany.assasa@gmail.com>
 */
// #include <vector>


//========== 雙AP隨機移動 =============
//========== 雙AP隨機移動 =============
//========== 雙AP隨機移動 =============
//========== 雙AP隨機移動 =============
//========== 雙AP隨機移動 =============
//========== 同時把 STA 的 mmWave Node 整合成 "一個 Node 兩個 interface" =============

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
  cout << "GetAny = " << Ipv4Address::GetAny () << endl;
  for (uint32_t i = 0; i < STA_etherNodes.GetN(); i++) {
    sinkApps.Add (sinkHelper.Install (STA_etherNodes.Get(i))); // Stas Node 是從 1 開始, 為了跳過代表 AP 的 0.
    sinkApps.Start (Seconds (0.0));
    sinkApps.Stop (Seconds (simulationTime));
  }
  cout << "?????" << endl;
  for (uint32_t i = 0; i < STA_etherNodes.GetN(); i++) {
    OnOffHelper src (socketType, InetSocketAddress (STA_ethInterface.GetAddress (i*2+1), port));
    cout << "Destination IP: " << STA_ethInterface.GetAddress (i*2+1) << endl;
    cout << "Destination Port: " << port << endl;
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
    std::tuple<double, uint16_t, uint16_t> max_snr_sector = staWifiMac[i]->HCC_PrintSnrTable(snrFileName);
    max_snr[i] = std::get<0>(max_snr_sector);
    uint16_t max_sector = std::get<1>(max_snr_sector);
    uint16_t active_sector = std::get<2>(max_snr_sector);
    // if (max_snr[i] < 6 && RT_enable==true && thr[i]==0) staWifiMac [i]->Perform_TXSS_TXOP (apWifiMac->GetAddress ());
    if (RT_enable && active_sector!=max_sector) {
      staWifiMac [i]->Perform_TXSS_TXOP (apWifiMac_left->GetAddress ());
      std::cout << "STA " << i << "left performs TXSS TXOP" << endl;
    }
    double angle = CalculateAngle(staNode, apNode); 
    file << Simulator::Now ().GetSeconds () << "," << thr[i] << "," << max_sector << "," << max_snr[i] << "," << angle << "," << active_sector << endl;
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
    std::tuple<double, uint16_t, uint16_t> max_snr_sector = staWifiMac[i]->HCC_PrintSnrTable(snrFileName);
    max_snr[i] = std::get<0>(max_snr_sector);
    uint16_t max_sector = std::get<1>(max_snr_sector);
    uint16_t active_sector = std::get<2>(max_snr_sector);
    // if (max_snr[i] < 6 && RT_enable==true && thr[i]==0) staWifiMac [i]->Perform_TXSS_TXOP (apWifiMac->GetAddress ());
    if (max_snr[i] < 6 && RT_enable && abs(active_sector-max_sector)!=4) {
      staWifiMac [i]->Perform_TXSS_TXOP (apWifiMac_right->GetAddress ());
      std::cout << "STA " << i << "right performs TXSS TXOP" << endl;
    }
    double angle = CalculateAngle(staNode, apNode); 
    file << Simulator::Now ().GetSeconds () << "," << thr[i] << "," << max_sector << "," << max_snr[i] << "," << angle << "," << active_sector << endl;
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
    left_file << "Time [s],Throughput [Mbps],Max SectorID,Max SNR[dB],Angle[deg],Active SectorID" << std::endl;
    left_file.close();
    right_file << "Time [s],Throughput [Mbps],Max SectorID,Max SNR[dB],Angle[deg],Active SectorID" << std::endl;
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
      std::cout << "DMG STA " << wifiMac->GetAddress () << " completed SLS phase with DMG AP" << attributes.peerStation << std::endl;
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

void print_position(Ptr<Node> node, int id, string filename) {
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
  ofstream file(filename, ios::app);
  file << Simulator::Now ().GetSeconds () << "," << mobility->GetPosition ().x << "," << mobility->GetPosition ().y << std::endl;
  file.close();
  Simulator::Schedule (Seconds (0.1), &print_position, node, id, filename);
}

void start_random_walk(Ptr<Node> node) {
  Ptr<RandomWalk2dMobilityModel> mobility = node->GetObject<RandomWalk2dMobilityModel>();
  mobility->SetAttribute("Mode", StringValue("Time"));
  mobility->SetAttribute("Time", TimeValue(Seconds(1.0)));
  mobility->SetAttribute("Speed", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"));
}

int
main (int argc, char *argv[])
{

  LogComponentEnable ("GlobalRouter", LOG_LEVEL_ALL);
  LogComponentEnable ("Ipv4GlobalRouting", LOG_LEVEL_ALL);
  //string applicationType = "bulk";              /* Type of the Tx application */
  bool activateApp = true;                      /* Flag to indicate whether we activate onoff or bulk App */
  string socketType = "ns3::TcpSocketFactory";  /* Socket Type (TCP/UDP) */
  uint32_t packetSize = 1400;                   /* Application payload size in bytes. */
  string dataRate = "300Mbps";                  /* Application data rate. */
  // string tcpVariant = "NewReno";                /* TCP Variant Type. */
  string tcpVariant = "Bic";                /* TCP Variant Type. */
  uint32_t bufferSize = 0.45 * 1024 * 1024;                 /* TCP Send/Receive Buffer Size. */
  uint32_t maxPackets = 0;                      /* Maximum Number of Packets */
  string msduAggSize = "max";                     /* The maximum aggregation size for A-MSDU in Bytes. */
  string mpduAggSize = "max";                  /* The maximum aggregation size for A-MSPU in Bytes. */
  // string mpduAggSize = "20000";                  /* The maximum aggregation size for A-MSPU in Bytes. */
  string queueSize = "4000p";                   /* Wifi MAC Queue Size. */
  string phyMode = "EDMG_OFDM_MCS8";                 /* Type of the Physical Layer. */
  bool verbose = false;                         /* Print Logging Information. */
  double simulationTime = 10;                   /* Simulation time in seconds. */
  bool pcapTracing = false;                     /* PCAP Tracing is enabled or not. */

  /* Command line argument parser setup. */
  CommandLine cmd;
  //HCC parameters
  //--------------------------------
  string file_dir;
  int user_num = 2;
  string comment;
  bool random_walk = false;
  uint32_t seed = 1;
  bool mac_retx_enable = true;
  //--------------------------------
  cmd.AddValue ("ue", "The number of STAs", user_num);
  cmd.AddValue ("fileDir", "The directory to store the throughput files", file_dir);
  cmd.AddValue ("comment", "The comment of the experiment", comment);
  cmd.AddValue ("activateApp", "Whether to activate data transmission or not", activateApp);
  cmd.AddValue ("vel", "The velocity of STA0", vel);
  cmd.AddValue ("retrain","Whether to retrain the beamforming or not", RT_enable);
  cmd.AddValue ("random_walk", "Set the moving path as random walk", random_walk);
  cmd.AddValue ("seed", "The seed of the experiment", seed);
  cmd.AddValue ("mac_retx_enable", "Whether to enable MAC retransmission or not", mac_retx_enable);
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


  RngSeedManager::SetSeed (seed);
  /* Validate A-MSDU and A-MPDU values */
  ValidateFrameAggregationAttributes (msduAggSize, mpduAggSize);
  /* Configure RTS/CTS and Fragmentation */
  ConfigureRtsCtsAndFragmenatation (false,0);
  /* Wifi MAC Queue Parameters */
  ChangeQueueSize (queueSize);

  /*** Configure TCP Options ***/
  if (mac_retx_enable)  Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1));
  

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
  
  // Create two AP nodes
  NodeContainer AP_wifiNodes;
  AP_wifiNodes.Create (2);
  Ptr<Node> leftAPWifi_node = AP_wifiNodes.Get (0);
  Ptr<Node> rightAPWifi_node = AP_wifiNodes.Get (1);
 
  // Create STAs mmWave nodes
  NodeContainer STA_wifiNodes;
  STA_wifiNodes.Create (user_num);

  NodeContainer STA_etherNodes;
  STA_etherNodes.Create (user_num);

  // merge all mmWave nodes
  NodeContainer allwifiNodes;
  allwifiNodes.Add (AP_wifiNodes);
  allwifiNodes.Add (STA_wifiNodes);
  cout << "allwifiNodes size: " << allwifiNodes.GetN() << endl;

  std::vector<NodeContainer> leftSTA_p2ppairs;
  std::vector<NodeContainer> rightSTA_p2ppairs;
  for (int i = 0; i < user_num; i++) {
    leftSTA_p2ppairs.push_back(NodeContainer (STA_wifiNodes.Get (i),STA_etherNodes.Get (i)));
    rightSTA_p2ppairs.push_back(NodeContainer (STA_wifiNodes.Get (i),STA_etherNodes.Get (i)));
  }

  PointToPointHelper STA_p2phelper;
  STA_p2phelper.SetDeviceAttribute ("DataRate", StringValue ("5Gb/s"));
  STA_p2phelper.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (0.000)));
  STA_p2phelper.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("4294967295p"));
  NetDeviceContainer leftSTA_p2p_devices;
  NetDeviceContainer rightSTA_p2p_devices;
  for (int i = 0; i < user_num; i++) {
    leftSTA_p2p_devices.Add (STA_p2phelper.Install (leftSTA_p2ppairs[i]));
    rightSTA_p2p_devices.Add (STA_p2phelper.Install (rightSTA_p2ppairs[i]));
  }

  /**** Set up Channel ****/
  DmgWifiChannelHelper wifiChannel_60480 ;
  wifiChannel_60480.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel_60480.AddPropagationLoss ("ns3::FriisPropagationLossModel", "Frequency", DoubleValue (60.48e9));

  DmgWifiChannelHelper wifiChannel_64800;
  wifiChannel_64800.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel_64800.AddPropagationLoss ("ns3::FriisPropagationLossModel", "Frequency", DoubleValue (64.80e9));

  /**** Setup physical layer ****/
  DmgWifiPhyHelper wifiPhy_60480 = DmgWifiPhyHelper::Default ();
  wifiPhy_60480.SetChannel (wifiChannel_60480.Create ());
  wifiPhy_60480.Set ("TxPowerStart", DoubleValue (10.0));
  wifiPhy_60480.Set ("TxPowerEnd", DoubleValue (10.0));
  wifiPhy_60480.Set ("TxPowerLevels", UintegerValue (1));
  wifiPhy_60480.Set ("ChannelNumber", UintegerValue (2));
  wifiPhy_60480.Set ("SupportOfdmPhy", BooleanValue (true));
  wifiPhy_60480.SetErrorRateModel ("ns3::DmgErrorModel", "FileName", StringValue ("WigigFiles/ErrorModel/LookupTable_1458.txt"));
  
  DmgWifiPhyHelper wifiPhy_64800 = DmgWifiPhyHelper::Default ();
  wifiPhy_64800.SetChannel (wifiChannel_64800.Create ());
  wifiPhy_64800.Set ("TxPowerStart", DoubleValue (10.0));
  wifiPhy_64800.Set ("TxPowerEnd", DoubleValue (10.0));
  wifiPhy_64800.Set ("TxPowerLevels", UintegerValue (1));
  wifiPhy_64800.Set ("ChannelNumber", UintegerValue (2));
  wifiPhy_64800.Set ("SupportOfdmPhy", BooleanValue (true));
  wifiPhy_64800.SetErrorRateModel ("ns3::DmgErrorModel", "FileName", StringValue ("WigigFiles/ErrorModel/LookupTable_1458.txt"));

  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue (phyMode));
  // wifi.SetRemoteStationManager ("ns3::IdealWifiManager", "BerThreshold", DoubleValue (1e-6));

  /* Add a DMG upper mac */
  DmgWifiMacHelper left_WifiMac = DmgWifiMacHelper::Default ();
  DmgWifiMacHelper right_WifiMac = DmgWifiMacHelper::Default ();

  /* Install DMG PCP/AP Node */
  Ssid leftSsid = Ssid ("Left");
  Ssid rightSsid = Ssid ("Right");
  Ssid failed = Ssid ("Failed");

  left_WifiMac.SetType ("ns3::DmgApWifiMac",
                        "Ssid", SsidValue(leftSsid),
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
  NetDeviceContainer ap_wifiDevice;
  ap_wifiDevice.Add (wifi.Install (wifiPhy_60480, left_WifiMac, leftAPWifi_node));//左側的AP
  std::cout << "left AP mac address: " << ap_wifiDevice.Get (0)->GetAddress () << endl;
  left_WifiMac.SetType ("ns3::DmgStaWifiMac",
                        "Ssid", SsidValue (leftSsid), "ActiveProbing", BooleanValue (false),
                        "EDMGSupported", BooleanValue (true),
                        "BE_MaxAmpduSize", StringValue (mpduAggSize),
                        "BE_MaxAmsduSize", StringValue (msduAggSize));

  NetDeviceContainer leftSTA_wifiDevice;
  for (int i = 0; i < user_num; i++) {
    leftSTA_wifiDevice.Add (wifi.Install (wifiPhy_60480, left_WifiMac, STA_wifiNodes.Get (i)));
    std::cout << "STA " << i << " mac address: " << leftSTA_wifiDevice.Get (i)->GetAddress () << endl;
  }

  right_WifiMac.SetType ("ns3::DmgApWifiMac",
                        "Ssid", SsidValue(rightSsid),
                        // "Ssid", SsidValue(failed),  
                        "BE_MaxAmpduSize", StringValue (mpduAggSize),
                        "BE_MaxAmsduSize", StringValue (msduAggSize),
                        "SSSlotsPerABFT", UintegerValue (8), "SSFramesPerSlot", UintegerValue (8),
                        "EDMGSupported", BooleanValue (true),
                        "BeaconInterval", TimeValue (MicroSeconds (102400)));
  
  // wifi.SetCodebook ("ns3::CodebookAnalytical",
  //                   "CodebookType", EnumValue (SIMPLE_CODEBOOK),
  //                   "Antennas", UintegerValue (1),
  //                   "Sectors", UintegerValue (8));

  ap_wifiDevice.Add (wifi.Install (wifiPhy_64800, right_WifiMac, rightAPWifi_node));//右側的AP
  std::cout << "right AP mac address: " << ap_wifiDevice.Get (1)->GetAddress () << endl;

  right_WifiMac.SetType ("ns3::DmgStaWifiMac",
                        "Ssid", SsidValue (rightSsid), "ActiveProbing", BooleanValue (false),
                        "EDMGSupported", BooleanValue (true),
                        "BE_MaxAmpduSize", StringValue (mpduAggSize),
                        "BE_MaxAmsduSize", StringValue (msduAggSize));
                        
  NetDeviceContainer rightSTA_wifiDevice;
  for (int i = 0; i < user_num; i++) {
    rightSTA_wifiDevice.Add (wifi.Install (wifiPhy_64800, right_WifiMac, STA_wifiNodes.Get (i)));
    std::cout << "STA " << i << " mac address: " << rightSTA_wifiDevice.Get (i)->GetAddress () << endl;
  }

  /* Setting mobility model */
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  if (random_walk){
    positionAlloc->Add (Vector (-1.0, 0.0, 5.0));   /* DMG PCP/AP */
    positionAlloc->Add (Vector (1.0, 0.0, 5.0));   /* DMG PCP/AP */
  }
  else{
    positionAlloc->Add (Vector (-1.0, 1.0, 5.0));   /* DMG PCP/AP */
    positionAlloc->Add (Vector (1.0, 1.0, 5.0));   /* DMG PCP/AP */
  }
  
  // Position of STA_etherNodes
  for (uint32_t i = 0; i < STA_etherNodes.GetN(); i++) {
    if (i == 0){
      positionAlloc->Add (Vector (-1.0, 0.0, 0.0));   /* DMG STA0*/
    }
    else{
      positionAlloc->Add (Vector (0.0, 0.0, 0.0));   /* DMG STA i_th*/
    }
  }
  // Position of STA_wifiNodes
  for (uint32_t i = 0; i < STA_wifiNodes.GetN(); i++) {
    if (i == 0){
      positionAlloc->Add (Vector (0.0, 0.0, 0.0));   /* DMG STA0*/
    }
    else{
      positionAlloc->Add (Vector (0.0, 0.0, 0.0));   /* DMG STA i_th*/
    }
  }


  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (AP_wifiNodes); //APs
  mobility.Install (STA_etherNodes); //STAs
  if (random_walk){
    mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue ("Time"),
                             "Time", StringValue ("1s"),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"),
                             "Bounds", StringValue ("-1.0|1.0|-1.0|1.0"));
  }
  mobility.Install (STA_wifiNodes); //STAs
  
  //STAs 不使用隨機移動時的固定移動設定
  if (!random_walk){
    Ptr<ConstantVelocityMobilityModel> sta0_mobility = STA_wifiNodes.Get (0)->GetObject<ConstantVelocityMobilityModel>();
    Ptr<ConstantVelocityMobilityModel> sta1_mobility = STA_wifiNodes.Get (1)->GetObject<ConstantVelocityMobilityModel>();

    //STA0 的移動
    Simulator::Schedule (Seconds (2.1), &ConstantVelocityMobilityModel::SetVelocity, sta0_mobility, Vector(vel, 0.0, 0.0));
    Simulator::Schedule (Seconds (6.1), &ConstantVelocityMobilityModel::SetVelocity, sta0_mobility, Vector(-vel, 0.0, 0.0));
    Simulator::Schedule (Seconds (14.1), &ConstantVelocityMobilityModel::SetVelocity, sta0_mobility, Vector(vel, 0.0, 0.0));
    Simulator::Schedule (Seconds (18.1), &ConstantVelocityMobilityModel::SetVelocity, sta0_mobility, Vector(0.0, 0.0, 0.0));

    //STA1 的移動
    // Simulator::Schedule (Seconds (4.1), &ConstantVelocityMobilityModel::SetVelocity, sta1_mobility, Vector(-vel, 0.0, 0.0));
    // Simulator::Schedule (Seconds (8.1), &ConstantVelocityMobilityModel::SetVelocity, sta1_mobility, Vector(vel, 0.0, 0.0));
    // Simulator::Schedule (Seconds (16.1), &ConstantVelocityMobilityModel::SetVelocity, sta1_mobility, Vector(-vel, 0.0, 0.0));
    // Simulator::Schedule (Seconds (20.1), &ConstantVelocityMobilityModel::SetVelocity, sta1_mobility, Vector(0.0, 0.0, 0.0));
  }
  
  //STA0's mobility model
  for (uint32_t i = 0; i < STA_wifiNodes.GetN(); i++) {
    string filename = file_dir + "position_STA_" + to_string(i) + ".csv";
    ofstream file(filename, ios::out);
    file << "Time [s],X [m],Y [m]" << endl;
    file.close();
    Simulator::Schedule (Seconds (0.1), &print_position, STA_wifiNodes.Get (i),i,filename);
  }
  if (random_walk){
    Simulator::Schedule (Seconds (2.1), &start_random_walk, STA_wifiNodes.Get (0));
  }

  Ptr<ConstantVelocityMobilityModel> ap_left_mobility = AP_wifiNodes.Get (0)->GetObject<ConstantVelocityMobilityModel>();
  Ptr<ConstantVelocityMobilityModel> ap_right_mobility = AP_wifiNodes.Get (1)->GetObject<ConstantVelocityMobilityModel>();
  cout << "position of left AP: " << ap_left_mobility->GetPosition () << endl;
  cout << "position of right AP: " << ap_right_mobility->GetPosition () << endl;
  
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
  stack.Install (AP_wifiNodes);
  stack.Install (STA_wifiNodes);
  stack.Install (STA_etherNodes);
  // 先分配 AP 和 mmWave 的連接
  Ipv4AddressHelper address;
  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer ap_leftInterfaces;
  ap_leftInterfaces.Add (address.Assign (ap_wifiDevice.Get(0)));
  Ipv4InterfaceContainer STA_LeftInterfaces;
  STA_LeftInterfaces = address.Assign (leftSTA_wifiDevice);
  
  address.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer ap_rightInterfaces;
  ap_rightInterfaces.Add (address.Assign (ap_wifiDevice.Get(1)));
  Ipv4InterfaceContainer STA_RightInterfaces;
  STA_RightInterfaces = address.Assign (rightSTA_wifiDevice);
  
  address.SetBase ("10.1.6.0", "255.255.255.0");
  Ipv4InterfaceContainer leftSTA_p2pInterfaces;
  leftSTA_p2pInterfaces = address.Assign (leftSTA_p2p_devices);
  address.SetBase ("10.1.7.0", "255.255.255.0");
  Ipv4InterfaceContainer rightSTA_p2pInterfaces;
  rightSTA_p2pInterfaces = address.Assign (rightSTA_p2p_devices);

  for (uint32_t i = 0; i < STA_LeftInterfaces.GetN(); i++) {
    cout << "STA " << i << " Left mmWave IP: " << STA_LeftInterfaces.GetAddress (i) << endl;
  }
  for (uint32_t i = 0; i < STA_RightInterfaces.GetN(); i++) {
    cout << "STA " << i << " Right mmWave IP: " << STA_RightInterfaces.GetAddress (i) << endl;
  } 
  for (uint32_t i = 0; i < leftSTA_p2pInterfaces.GetN()/2; i++) {
    cout << "STA wifiNode " << i << " Left p2p IP: " << leftSTA_p2pInterfaces.GetAddress (2*i) << endl;
    cout << "STA etherNode " << i << " Left p2p IP: " << leftSTA_p2pInterfaces.GetAddress (2*i+1) << endl;
  }
  for (uint32_t i = 0; i < rightSTA_p2pInterfaces.GetN()/2; i++) {
    cout << "STA wifiNode " << i << " Right p2p IP: " << rightSTA_p2pInterfaces.GetAddress (2*i) << endl;
    cout << "STA etherNode " << i << " Right p2p IP: " << rightSTA_p2pInterfaces.GetAddress (2*i+1) << endl;
  }
  
  cout << "IP has been assigned" << endl;
  
  cout << "NodeList::GetNNodes(): " << NodeList::GetNNodes() << endl;
  // return 0;
  
  // 在 PopulateRoutingTables 之前調用
  std::cout << "\n=== AP Nodes Interfaces ===" << std::endl;
  for (uint32_t i = 0; i <AP_wifiNodes.GetN(); i++) {
      PrintInterfaceInfo(AP_wifiNodes.Get(i));
  }

  std::cout << "\n=== mmWave Nodes Interfaces ===" << std::endl;
  for (uint32_t i = 0; i < STA_wifiNodes.GetN(); i++) {
      PrintInterfaceInfo(STA_wifiNodes.Get(i));
  }

  // std::cout << "\n=== UE Nodes Interfaces ===" << std::endl;
  // for (uint32_t i = 0; i < STA_ethNodes.GetN(); i++) {
  //     PrintInterfaceInfo(STA_ethNodes.Get(i));
  // }
  

  /* Populate routing table */
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  /* We do not want any ARP packets */
  PopulateArpCache ();


  // return 0; // node ~ IP 相關設定檢查點
  
  if (activateApp)
    {
      /* Install Simple UDP Server on the DMG AP */
      

      left_apps = new_InstallPacketSink(dataRate, leftAPWifi_node, STA_etherNodes , leftSTA_p2pInterfaces, socketType, simulationTime);
      right_apps = new_InstallPacketSink(dataRate, rightAPWifi_node, STA_etherNodes , rightSTA_p2pInterfaces, socketType, simulationTime);
      // right_apps = new_InstallPacketSink(dataRate, rightAPWifi_node, rightSTA_nodes , STA_RightInterfaces, socketType, simulationTime);
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
      wifiPhy_60480.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
      wifiPhy_60480.EnablePcap ("Traces/AccessPoint_left", ap_wifiDevice.Get(0), false);
      wifiPhy_60480.EnablePcap ("Traces/StaNode_left", leftSTA_wifiDevice, false);

      wifiPhy_64800.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
      wifiPhy_64800.EnablePcap ("Traces/AccessPoint_right", ap_wifiDevice.Get(1), false);
      wifiPhy_64800.EnablePcap ("Traces/StaNode_right", rightSTA_wifiDevice, false);
      
    }
  // return 0; // 安裝 pcap 相關設定檢查點
  /* Stations */
  Ptr<WifiNetDevice> apWifiNetDevice_left = StaticCast<WifiNetDevice> (ap_wifiDevice.Get (0));
  Ptr<WifiNetDevice> apWifiNetDevice_right = StaticCast<WifiNetDevice> (ap_wifiDevice.Get (1));
  Ptr<WifiNetDevice> staWifiNetDevice_left[user_num];
  Ptr<WifiNetDevice> staWifiNetDevice_right[user_num];
  for (int i = 0; i < user_num; i++) {
    staWifiNetDevice_left[i] = StaticCast<WifiNetDevice> (leftSTA_wifiDevice.Get (i));
    staWifiNetDevice_right[i] = StaticCast<WifiNetDevice> (rightSTA_wifiDevice.Get (i));
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
  for (int i = 0; i < user_num; i++) {
    staWifiMac_left[i]->TraceConnectWithoutContext ("SLSCompleted", MakeBoundCallback (&SLSCompleted, staWifiMac_left[i]));
    staWifiMac_right[i]->TraceConnectWithoutContext ("SLSCompleted", MakeBoundCallback (&SLSCompleted, staWifiMac_right[i]));
  }


  apWifiMac_left->GetCodebook ()->TraceConnectWithoutContext ("ActiveTxSectorID", MakeBoundCallback (&ActiveTxSectorIDChanged, apWifiMac_left));
  apWifiMac_right->GetCodebook ()->TraceConnectWithoutContext ("ActiveTxSectorID", MakeBoundCallback (&ActiveTxSectorIDChanged, apWifiMac_right));

  for (int i = 0; i < user_num; i++) {  
    staWifiMac_left[i]->GetCodebook ()->TraceConnectWithoutContext ("ActiveTxSectorID", MakeBoundCallback (&ActiveTxSectorIDChanged, staWifiMac_left[i]));
    staWifiMac_right[i]->GetCodebook ()->TraceConnectWithoutContext ("ActiveTxSectorID", MakeBoundCallback (&ActiveTxSectorIDChanged, staWifiMac_right[i]));
  }

  //撤定額外  mcs
  Ptr<WifiRemoteStationManager> manager = staWifiMac_left[0]->GetWifiRemoteStationManager();
  manager->AddSupportedMcs (staWifiMac_left[0]->GetAddress(), WifiMode("EDMG_OFDM_MCS4"));
  FlowMonitorHelper flowmon;
  if (activateApp)
    {
      /* Install FlowMonitor on all nodes */
      monitor = flowmon.Install (allwifiNodes);

      /* Schedule Throughput Calulcations */
      string filename = file_dir + "arp_cache_left.txt";
      Simulator::Schedule (Seconds (0.0), &new_CreateFile, file_dir, user_num);
      string retrain_str = RT_enable ? "enabled" : "disabled";
      cout << "CHECK retrain is " << retrain_str << endl;
      Simulator::Schedule (Seconds (0.1), &left_new_CalculateThroughput, file_dir,leftAPWifi_node,STA_etherNodes,left_apps,staWifiMac_left,true);
      Simulator::Schedule (Seconds (0.1), &right_new_CalculateThroughput, file_dir,rightAPWifi_node,STA_etherNodes,right_apps,staWifiMac_right,false);
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
