#include "net/quic/congestion_control/pcc_sender.h"

#include <stdio.h>

namespace net {

PCCSender::PCCSender(const RttStats* rtt_stats)
  : rtt_stats_(rtt_stats),
    current_monitor_(-1),
    previous_monitor_(-1),
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

        current_monitor_end_time_ == NULL;
      }
    }

    PacketInfo packet_info = {sent_time, bytes};
    mointors_[current_monitor_].total_packet_map = packet_info;
    seq_monitor_map_[sequence_number] = current_monitor_;

  return true;
}

void PCCSender::start_monitor(QuicTime sent_time){
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

}

void PCCSender::OnCongestionEvent(
    bool rtt_updated,
    QuicByteCount bytes_in_flight,
    const CongestionVector& acked_packets,
    const CongestionVector& lost_packets) {

  for (CongestionVector::const_iterator it = lost_packets.begin();
       it != lost_packets.end(); ++it) {
    MonitorNumber monitor_num = seq_monitor_map_[it->first];
    PacketInfo packet_info = {it->second->sent_time, it->second->bytes_sent};
    mointors_[monitor_num].ack_packet_map[it->first] = packet_info;

    end_monitor(it->first);
    seq_monitor_map_.erase(it->first);
  }
  for (CongestionVector::const_iterator it = acked_packets.begin();
       it != acked_packets.end(); ++it) {
    MonitorNumber monitor_num = seq_monitor_map_[it->first];
    PacketInfo packet_info = {it->second->sent_time, it->second->bytes_sent};
    mointors_[monitor_num].lost_packet_map[it->first] = packet_info;

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

PCCUtility::PCCUtility()
  : current_rate_(1),
    prevoius_rate_(1),
    previous_utility_(0),
    previous_rtt_(0),
    if_starting_phase_(false),
    if_make_guess_(false),
    if_recording_guess_(false),
    num_recorded_(0),
    guess_time_(0),
    continous_guess_count_(0),
    tartger_monitor_(0),
    if_moving_phase_(false),
    if_initial_moving_phase_(false),
    change_direction_(0),
    change_intense_(1) {
    printf("pcc_utility\n");
  }

void PCCUtility::onMonitorStart(MonitorNumber current_monitor) {
  if (if_starting_phase_) {
    prevoius_rate_ = current_rate_;
    current_rate_ *= 2;

    return;
  }

  if (if_make_guess_) {
    if (guess_time_ == 0 && continous_guess_count_ == MAX_COUNTINOUS_GUESS) {
      continous_guess_count_ = 0;
    }

    if (guess_time_ == 0) {
      if_recording_guess_ = true;

      continous_guess_count_++;

      for (int i = 0; i < NUMBER_OF_PROBE; i += 2) {
        int rand_dir = rand() % 2 * 2 - 1;

        guess_stat_bucket[i].rate = current_rate_ + rand_dir * continous_guess_count_ * GRANULARITY * current_rate_;
        guess_stat_bucket[i + 1].rate = current_rate_ - rand_dir * continous_guess_count_ * GRANULARITY * current_rate_;  
  
      }

      for (int i = 0; i < NUMBER_OF_PROBE; i++) {
        guess_stat_bucket[i].monitor = (current_monitor + i) % NUM_MONITOR;
      }
    }

    current_rate_ = guess_stat_bucket[guess_time_].rate;
    guess_time_++;

    if (guess_time_ == NUMBER_OF_PROBE) {
      if_make_guess_ = false;
      guess_time_ = 0;
    }
  }
}

}  // namespace net




