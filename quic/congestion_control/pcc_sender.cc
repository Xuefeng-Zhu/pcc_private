#include "net/quic/congestion_control/pcc_sender.h"

#include <stdio.h>

namespace net {

PCCSender::PCCSender(const RttStats* rtt_stats)
	: rtt_stats_(rtt_stats),
    // if_monitor_(false),
    current_monitor_(-1),
    previous_monitor_(-1),
    // monitor_left_(0),
    current_monitor_end_time_(NULL){
		printf("pcc\n");
}

PCCSender::~PCCSender() {}

void PCCSender::SetFromConfig(const QuicConfig& config,
                                 bool is_server,
                                 bool using_pacing) {
}

bool PCCSender::ResumeConnectionState(
  const CachedNetworkParameters& cached_network_params) {
  return true;
}

void PCCSender::SetNumEmulatedConnections(int num_connections) {
  
}

bool PCCSender::OnPacketSent(
    QuicTime sent_time,
    QuicByteCount bytes_in_flight,
    QuicPacketSequenceNumber sequence_number,
    QuicByteCount bytes,
    HasRetransmittableData has_retransmittable_data) {

    // TODO : case for retransmission
    if (current_monitor_end_time_ == NULL) {
      start_monitor();
    } else {
      QuicTime::Delta diff = sent_time.Subtract(current_monitor_end_time_);
      if (diff.ToMicroseconds() > 0) {
        mointors_[current_monitor_].end_transmission_time = sent_time;
        mointors_[current_monitor_].state = WAITING;
        end_seq_monitor_map_[sequence_number] = current_monitor_;
        // monitor_left_++;

        current_monitor_end_time_ == NULL;
      }
    }

    mointors_[current_monitor_].total++;
    seq_monitor_map_[sequence_number] = current_monitor_;

  return true;
}

void PCCSender::start_monitor(QuicTime sent_time){
  // previous_monitor_ = current_monitor_;
  current_monitor_ = (current_monitor_ + 1) % NUM_MONITOR;
  // TODO : on MonitorStart  

  // calculate monitor interval and monitor end time
  double rand_factor = double(rand() % 3) / 10;
  int64 srtt = rtt_stats_.smoothed_rtt().ToMicroseconds();
  if (srtt == 0) {
    srtt = rtt_stats_.initial_rtt_us();
  }
  QuicTime::Delta monitor_interval = 
      QuicTime::Delta::FromMicroseconds(srtt * (1.5 + rand_factor));
  current_monitor_end_time_ = sent_time.Add(monitor_interval);

  mointors_[current_monitor_].state = SENDING;
  mointors_[current_monitor_].ack = 0;
  mointors_[current_monitor_].lost = 0;
  mointors_[current_monitor_].total = 0;
  mointors_[current_monitor_].start_time = sent_time;
  mointors_[current_monitor_].end_time = NULL;
  mointors_[current_monitor_].end_transmission_time = NULL;

  // if_monitor_ = true;
}

void PCCSender::OnCongestionEvent(
    bool rtt_updated,
    QuicByteCount bytes_in_flight,
    const CongestionVector& acked_packets,
    const CongestionVector& lost_packets) {

  for (CongestionVector::const_iterator it = lost_packets.begin();
       it != lost_packets.end(); ++it) {
    MonitorNumber monitor_num = seq_monitor_map_[it->first];
    mointors_[monitor_num].ack++;
    end_monitor(it->first);
    seq_monitor_map_.erase(it->first);
  }
  for (CongestionVector::const_iterator it = acked_packets.begin();
       it != acked_packets.end(); ++it) {
    end_monitor(it->first);
    seq_monitor_map_.erase(it->first);
  }
}

void end_monitor(QuicPacketSequenceNumber sequence_number) {
  std::map<QuicPacketSequenceNumber, MonitorNumber>::iterator it =
      end_seq_monitor_map_.find(sequence_number);
  if (it != end_seq_monitor_map_.end()){
    MonitorNumber prev_monitor_num = it->second;
    if (mointors_[prev_monitor_num].state == WAITING){

      mointors_[prev_monitor_num].lost = 
          mointors_[prev_monitor_num].total - mointors_[prev_monitor_num].ack;
      mointors_[prev_monitor_num].state = FINISHED;
      // TODO : onMonitorEnd
    }
    end_seq_monitor_map_.erase(sequence_number);
  }
}

void PCCSender::OnRetransmissionTimeout(bool packets_retransmitted) {

}

void PCCSender::RevertRetransmissionTimeout() {

}

QuicTime::Delta PCCSender::TimeUntilSend(
      QuicTime now,
      QuicByteCount bytes_in_flight,
      HasRetransmittableData has_retransmittable_data) const {
  return QuicTime::Delta::Zero();
  //printf("pcc pacing\n");
  if (ideal_next_packet_send_time_ > now.Add(alarm_granularity_)) {
    //DVLOG(1) << "Delaying packet: "
    //         << ideal_next_packet_send_time_.Subtract(now).ToMicroseconds();
    was_last_send_delayed_ = true;
    return ideal_next_packet_send_time_.Subtract(now);
  }
  return QuicTime::Delta::Zero();
}

QuicBandwidth PCCSender::PacingRate() const {
  return QuicBandwidth::Zero();
}

QuicBandwidth PCCSender::BandwidthEstimate() const {
  return QuicBandwidth::Zero();
}

bool PCCSender::HasReliableBandwidthEstimate() const {
  return false;
}

QuicTime::Delta PCCSender::RetransmissionDelay() const {
  return QuicTime::Delta::Zero();
}

QuicByteCount PCCSender::GetCongestionWindow() const {
  return 1000*kMaxPacketSize;
}

bool PCCSender::InSlowStart() const {
  return false;
}

bool PCCSender::InRecovery() const {
  return false;
}

QuicByteCount PCCSender::GetSlowStartThreshold() const {
  return 1000*kMaxPacketSize;
}

CongestionControlType PCCSender::GetCongestionControlType() const {
  return kPcc;
}

}  // namespace net


