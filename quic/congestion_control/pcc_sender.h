#ifndef NET_QUIC_CONGESTION_CONTROL_PCC_SENDER_H_
#define NET_QUIC_CONGESTION_CONTROL_PCC_SENDER_H_

# include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "net/base/net_export.h"
#include "net/quic/congestion_control/cubic.h"
#include "net/quic/congestion_control/hybrid_slow_start.h"
#include "net/quic/congestion_control/prr_sender.h"
#include "net/quic/congestion_control/send_algorithm_interface.h"
#include "net/quic/crypto/cached_network_parameters.h"
#include "net/quic/quic_bandwidth.h"
#include "net/quic/quic_connection_stats.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_time.h"

namespace net {

typedef int MonitorNumber;

enum State {
  SENDING,
  WAITING,
  FINISHED
};

struct PacketInfo {
  QuicTime sent_time,
  QuicByteCount bytes
};

struct PCCMonitor {
  State state;

  // time statics  
  QuicTime start_time;
  QuicTime end_time;
  QuicTime end_transmission_time;

  // packet statics
  std::map<QuicPacketSequenceNumber , PacketInfo> total_packet_map;
  std::map<QuicPacketSequenceNumber , PacketInfo> ack_packet_map;
  std::map<QuicPacketSequenceNumber , PacketInfo> lost_packet_map;
};

const int NUM_MONITOR = 100;

class RttStats;

class NET_EXPORT_PRIVATE PCCSender : public SendAlgorithmInterface {
 public:
  PCCSender(const RttStats* rtt_stats);
  ~PCCSender() override;

  // SendAlgorithmInterface methods.
  void SetFromConfig(const QuicConfig& config,
                     bool is_server,
                     bool using_pacing) override;
  bool ResumeConnectionState(
      const CachedNetworkParameters& cached_network_params) override;
  void SetNumEmulatedConnections(int num_connections) override;
  void OnCongestionEvent(bool rtt_updated,
                         QuicByteCount bytes_in_flight,
                         const CongestionVector& acked_packets,
                         const CongestionVector& lost_packets) override;
  bool OnPacketSent(QuicTime sent_time,
                    QuicByteCount bytes_in_flight,
                    QuicPacketSequenceNumber sequence_number,
                    QuicByteCount bytes,
                    HasRetransmittableData is_retransmittable) override;
  void OnRetransmissionTimeout(bool packets_retransmitted) override;
  void RevertRetransmissionTimeout() override;
  QuicTime::Delta TimeUntilSend(
      QuicTime now,
      QuicByteCount bytes_in_flight,
      HasRetransmittableData has_retransmittable_data) const override;
  QuicBandwidth PacingRate() const override;
  QuicBandwidth BandwidthEstimate() const override;
  bool HasReliableBandwidthEstimate() const override;
  QuicTime::Delta RetransmissionDelay() const override;
  QuicByteCount GetCongestionWindow() const override;
  bool InSlowStart() const override;
  bool InRecovery() const override;
  QuicByteCount GetSlowStartThreshold() const override;
  CongestionControlType GetCongestionControlType() const override;
  // End implementation of SendAlgorithmInterface.

 private:
 	const QuicTime::Delta alarm_granularity_ = QuicTime::Delta::FromMilliseconds(1);

  // PCC monitor variable
  MonitorNumber current_monitor_;
  QuicTime current_monitor_end_time_;

  PCCMonitor monitors_[NUM_MONITOR];
  std::map<QuicPacketSequenceNumber, MonitorNumber> seq_monitor_map_;
  std::map<QuicPacketSequenceNumber, MonitorNumber> end_seq_monitor_map_;
  const RttStats* rtt_stats_;

  // private PCC functions
  // Start a new monitor
  void start_monitor(QuicTime sent_time);
  // End previous monitor
  void end_monitor(QuicPacketSequenceNumber sequence_number);


  DISALLOW_COPY_AND_ASSIGN(PCCSender);
};

}  // namespace net

#endif


