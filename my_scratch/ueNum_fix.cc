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
#include "common-functions.h"
#include <iomanip>


NS_LOG_COMPONENT_DEFINE ("BeamformingCBAP");

using namespace ns3;
using namespace std;

//HCC settings

double vel = 1.0;

bool RT_enable = false;




Ptr<DmgApWifiMac> apWifiMac;


std::pair<ApplicationContainer, ApplicationContainer> apps;

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
new_InstallPacketSink(string dataRate,NodeContainer Nodes, Ipv4InterfaceContainer sta_inters, string socketType, double simulationTime, int Total_user_num)
{
  ApplicationContainer sinkApps; 
  ApplicationContainer srcApps;
  Ptr<Node> apNode = Nodes.Get(0); // 在定義的時候, Nodes.Get(0) 就是 ap. 

  PacketSinkHelper sinkHelper (socketType, InetSocketAddress (Ipv4Address::GetAny (), 9999));
  for (int i = 0; i < Total_user_num; i++) {
    sinkApps.Add (sinkHelper.Install (Nodes.Get(i+1))); // Stas Node 是從 1 開始, 為了跳過代表 AP 的 0.
    sinkApps.Start (Seconds (0.0));
    sinkApps.Stop (Seconds (simulationTime));
  }
  
  for (int i = 0; i < Total_user_num; i++) {
    OnOffHelper src (socketType, InetSocketAddress (sta_inters.GetAddress (i), 9999));
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
  return apps;
}


void
new_CalculateThroughput (string file_dir, NodeContainer allNodes,std::vector<Ptr<DmgStaWifiMac>> staWifiMac, int ue_num)
{
  static std::vector<double> thr(ue_num, 0);
  static std::vector<uint64_t> totalRx(ue_num, 0);
  static std::vector<double> throughput(ue_num, 0);
  static std::vector<double> max_snr(ue_num, 0);

  for (int i = 0; i < ue_num; i++) {
    string file_name;
    string snrFileName;
    ApplicationContainer temp_appcon = apps.first.Get(i);
    Ptr<PacketSink> temp_sink = StaticCast<PacketSink> (temp_appcon.Get(0));
    file_name = file_dir + "STA_" + to_string(i) + ".csv";
    snrFileName = file_dir + "STA_" + to_string(i) + "_snr.txt";

    ofstream file(file_name, ios::app);
    Ptr<Node> staNode = allNodes.Get (i+1);
    Ptr<Node> apNode = allNodes.Get (0);
    
    thr[i] = CalculateSingleStreamThroughput (temp_sink, totalRx[i], throughput[i]);
    std::pair<double, uint16_t> max_snr_sector = staWifiMac[i]->HCC_PrintSnrTable(snrFileName);
    max_snr[i] = max_snr_sector.first;
    // if (max_snr[i] < 6 && RT_enable==true && thr[i]==0) staWifiMac [i]->Perform_TXSS_TXOP (apWifiMac->GetAddress ());
    if (max_snr[i] < 6 && RT_enable==true) staWifiMac [i]->Perform_TXSS_TXOP (apWifiMac->GetAddress ());
    uint16_t max_sector = max_snr_sector.second;
    double angle = CalculateAngle(staNode, apNode); 
    file << Simulator::Now ().GetSeconds () << "," << thr[i] << "," << max_sector << "," << max_snr[i] << "," << angle << endl;
    file.close();
  }

  string allThrFileName = file_dir + "all_thr.csv";
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
  Simulator::Schedule (MilliSeconds (100), &new_CalculateThroughput, file_dir, allNodes, staWifiMac, ue_num);
}



void 
CreateFile (string file_dir,NodeContainer allNodes,std::vector<Ptr<DmgStaWifiMac>> staWifiMac, int ue_num)
{
  cout << "Creating file" << endl;
  for (int i = 0; i < ue_num; i++) {
    string file_name;
    string snrFileName;
    file_name = file_dir + "STA_" + to_string(i) + ".csv";
    snrFileName = file_dir + "STA_" + to_string(i) + "_snr.txt";
    ofstream file(file_name, ios::out);
    ofstream snrFile(snrFileName, ios::out);
    file << "Time [s],Throughput [Mbps],Max SectorID,Max SNR[dB],Angle[deg]" << std::endl;
    file.close();
    snrFile.close();
  }
  string allThrFileName = file_dir + "all_thr.csv";
  ofstream allThrFile(allThrFileName, ios::out);
  allThrFile << "Time [s],";
  for (int i = 0; i < ue_num; i++) {
    allThrFile << "STA_" << i << "_Thr[Mbps]" << ",";
    allThrFile << "STA_" << i << "_MaxSNR[dB]" << ",";
  }
  allThrFile << "Non_Moving_Thr[Mbps]" << std::endl;
  allThrFile.close();
  // CalculateThroughput (file_dir, allNodes, ue_num); 
  new_CalculateThroughput (file_dir, allNodes, staWifiMac, ue_num); 
}


void
StationAssoicated (Ptr<DmgStaWifiMac> staWifiMac, Mac48Address address, uint16_t aid)
{
  std::cout << "DMG STA " << staWifiMac->GetAddress () << " associated with DMG AP " << address << std::endl;
  std::cout << "Association ID (AID) = " << aid << std::endl;
  staWifiMac->Perform_TXSS_TXOP (apWifiMac->GetAddress ());
}

void
SLSCompleted (Ptr<DmgWifiMac> wifiMac, SlsCompletionAttrbitutes attributes)
{
  if (attributes.accessPeriod == CHANNEL_ACCESS_BHI)
    {
      if (wifiMac == apWifiMac)
        {
          std::cout << "DMG AP " << apWifiMac->GetAddress () <<
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

int
main (int argc, char *argv[])
{
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
  int user_num = 8;
  string comment;
  bool circle = true;
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
  NodeContainer allNodes;
  allNodes.Create (1 + user_num);  //1 for AP, user_num for STAs


  /* Add a DMG upper mac */
  DmgWifiMacHelper wifiMac = DmgWifiMacHelper::Default ();

  /* Install DMG PCP/AP Node */
  Ssid ssid = Ssid ("BeamformingCBAP");
  wifiMac.SetType ("ns3::DmgApWifiMac",
                   "Ssid", SsidValue(ssid),
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
  apDevice = wifi.Install (wifiPhy, wifiMac, allNodes.Get (0));//AP

  /* Install DMG STA Nodes */
  wifiMac.SetType ("ns3::DmgStaWifiMac",
                   "Ssid", SsidValue (ssid), "ActiveProbing", BooleanValue (false),
                   "EDMGSupported", BooleanValue (true),
                   "BE_MaxAmpduSize", StringValue (mpduAggSize),
                   "BE_MaxAmsduSize", StringValue (msduAggSize));
  
  NetDeviceContainer staDevices;//存了所有sta 的netdevice list

  for (int i = 0; i < user_num; i++) {
    NetDeviceContainer staDevice_temp;
    staDevice_temp = wifi.Install (wifiPhy, wifiMac, allNodes.Get (i+1));//STAs
    staDevices.Add (staDevice_temp);
  }
  /* Setting mobility model */
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 5.0));   /* DMG PCP/AP */
  positionAlloc->Add (Vector (1.0, -1.0, 0.0));   /* DMG STA 0 */
  for (int i = 1; i < user_num; i++) {
    positionAlloc->Add (Vector (0.0, -1.0, 0.0));   /* DMG STA i_th*/
  }

  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (allNodes); //install all the nodes' mobility model, including the AP and the STAs

  Ptr<ConstantVelocityMobilityModel> apNode_mobility = allNodes.Get (0)->GetObject<ConstantVelocityMobilityModel>();
  Vector ap_vel = apNode_mobility->GetVelocity();
  cout << "AP velocity: " << ap_vel.x << ", " << ap_vel.y << ", " << ap_vel.z << endl;

  //STA0's mobility model
  Ptr<ConstantVelocityMobilityModel> sta0_mobility = allNodes.Get (1)->GetObject<ConstantVelocityMobilityModel>();

  // Simulator::Schedule (Seconds (2.1), &ConstantVelocityMobilityModel::SetVelocity, sta0_mobility, Vector(200.0, 0.0, 0.0));
  // Simulator::Schedule (Seconds (3.1), &ConstantVelocityMobilityModel::SetVelocity, sta0_mobility, Vector(0.0, 0.0, 0.0));
  // Simulator::Schedule (Seconds (4.1), &ConstantVelocityMobilityModel::SetVelocity, sta0_mobility, Vector(-200.0, 0.0, 0.0));
  // Simulator::Schedule (Seconds (5.1), &ConstantVelocityMobilityModel::SetVelocity, sta0_mobility, Vector(0.0, 0.0, 0.0));
  
  if (circle)
  {
    Simulator::Schedule (Seconds (3.1), &CircleMoving, allNodes.Get(0), allNodes.Get(1), vel);
  }
  else 
  {
    Simulator::Schedule (Seconds (2.1), &ConstantVelocityMobilityModel::SetVelocity, sta0_mobility, Vector(-vel, 0.0, 0.0));
    Simulator::Schedule (Seconds (10.1), &ConstantVelocityMobilityModel::SetVelocity, sta0_mobility, Vector(vel, 0.0, 0.0));
  }
  
  
  


  // Simulator::Schedule (Seconds (4.1), &ConstantVelocityMobilityModel::SetPosition, sta0_mobility, Vector(-vel, -1.0, 0.0));
  // Simulator::Schedule (Seconds (8.1), &ConstantVelocityMobilityModel::SetPosition, sta0_mobility, Vector(vel, -1.0, 0.0));


  /* Internet stack*/
  InternetStackHelper stack;
  stack.Install (allNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign (apDevice);
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign (staDevices);
  for (int i = 0; i < user_num; i++) {
    cout << "STA " << i << " IP: " << staInterfaces.GetAddress (i) << endl;
  }
  cout << "IP has been assigned" << endl;
  /* Populate routing table */
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  /* We do not want any ARP packets */
  PopulateArpCache ();

  if (activateApp)
    {
      /* Install Simple UDP Server on the DMG AP */
      

      apps = new_InstallPacketSink(dataRate, allNodes, staInterfaces, socketType, simulationTime, user_num);
      cout << "Packet sink has been installed" << endl;
      
      
      
    }

  Ptr<PacketSink> packetSink[user_num];
  Ptr<OnOffApplication> onoff[user_num];
  for (int i = 0; i < user_num; i++) {
        ApplicationContainer temp_appcon = apps.first.Get(i);
        Ptr<PacketSink> temp_sink = StaticCast<PacketSink> (temp_appcon.Get(0));
        ApplicationContainer temp_appcon2 = apps.second.Get(i);
        Ptr<OnOffApplication> temp_src = StaticCast<OnOffApplication> (temp_appcon2.Get(0));
        packetSink[i] = temp_sink;
        onoff[i] = temp_src;
       }

  /* Enable Traces */
  if (pcapTracing)
    {
      wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
      wifiPhy.EnablePcap ("Traces/AccessPoint_"+comment, apDevice, false);
      wifiPhy.EnablePcap ("Traces/StaNode_"+comment, staDevices, false);
    }

  /* Stations */
  Ptr<WifiNetDevice> apWifiNetDevice = StaticCast<WifiNetDevice> (apDevice.Get (0));
  Ptr<WifiNetDevice> staWifiNetDevice[user_num];
  for (int i = 0; i < user_num; i++) {
    staWifiNetDevice[i] = StaticCast<WifiNetDevice> (staDevices.Get (i));
  }

  apWifiMac = StaticCast<DmgApWifiMac> (apWifiNetDevice->GetMac ());
  std::vector<Ptr<DmgStaWifiMac>> staWifiMac(user_num);
  for (int i = 0; i < user_num; i++) {
    staWifiMac[i] = StaticCast<DmgStaWifiMac> (staWifiNetDevice[i]->GetMac ());
  } 

  /** Connect Traces **/
  for (int i = 0; i < user_num; i++) {
    staWifiMac[i]->TraceConnectWithoutContext ("Assoc", MakeBoundCallback (&StationAssoicated, staWifiMac[i]));
  }

  // apWifiMac->TraceConnectWithoutContext ("SLSCompleted", MakeBoundCallback (&SLSCompleted, apWifiMac));
  // for (int i = 0; i < user_num; i++) {
  //   staWifiMac[i]->TraceConnectWithoutContext ("SLSCompleted", MakeBoundCallback (&SLSCompleted, staWifiMac[i]));
  // }


  apWifiMac->GetCodebook ()->TraceConnectWithoutContext ("ActiveTxSectorID", MakeBoundCallback (&ActiveTxSectorIDChanged, apWifiMac));
  for (int i = 0; i < user_num; i++) {  
    staWifiMac[i]->GetCodebook ()->TraceConnectWithoutContext ("ActiveTxSectorID", MakeBoundCallback (&ActiveTxSectorIDChanged, staWifiMac[i]));
  }

  FlowMonitorHelper flowmon;
  if (activateApp)
    {
      /* Install FlowMonitor on all nodes */
      monitor = flowmon.Install (allNodes);

      /* Schedule Throughput Calulcations */
      Simulator::Schedule (Seconds (2.1), &CreateFile, file_dir, allNodes, staWifiMac, user_num);
    }
  /* Schedule many TXSS CBAPs during the data transmission interval. */
  
  Simulator::Schedule (Seconds (2.1), &print_current_time, allNodes.Get (1));


  Simulator::Stop (Seconds (simulationTime + 0.101));
  Simulator::Run ();
  Simulator::Destroy ();

  if (activateApp)
    {
      PrintFlowMonitorStatistics (flowmon, monitor, simulationTime - 1);

      /* Print Application Layer Results Summary */
      std::cout << "\nApplication Layer Statistics:" << std::endl;
      for (int i = 0; i < user_num; i++)
        {
          std::cout << "  Tx Packets: " << onoff[i]->GetTotalTxPackets () << std::endl;
          std::cout << "  Tx Bytes:   " << onoff[i]->GetTotalTxBytes () << std::endl;
        }
      for (int i = 0; i < user_num; i++)
        {
          std::cout << "----------------------------------------" << std::endl;
          std::cout << "User " << i << " Statistics:" << std::endl;
          std::cout << "  Rx Packets: " << packetSink[i]->GetTotalReceivedPackets () << std::endl;
          std::cout << "  Rx Bytes:   " << packetSink[i]->GetTotalRx () << std::endl;
          std::cout << "  Throughput: " << packetSink[i]->GetTotalRx () * 8.0 / ((simulationTime - 2.0) * 1e6) << " Mbps" << std::endl;
          std::cout << "----------------------------------------" << std::endl<<std::endl;
        }
    }

  

  return 0;
}
