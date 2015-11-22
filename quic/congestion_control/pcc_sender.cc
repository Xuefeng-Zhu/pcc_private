#include <stdio.h>

#include "net/quic/congestion_control/pcc_sender.h"
#include "net/quic/congestion_control/rtt_stats.h"
#include "base/time/time.h"

namespace net {

PCCMonitor::PCCMonitor()
  : start_time(QuicTime::Zero()),
    end_time(QuicTime::Zero()),
    end_transmission_time(QuicTime::Zero()),
    start_seq_num(-1),
    end_seq_num(-1){
}

PCCMonitor::~PCCMonitor(){
}

PCCSender::PCCSender(const RttStats* rtt_stats)
  : current_monitor_(-1),
    current_monitor_end_time_(QuicTime::Zero()),
    rtt_stats_(rtt_stats),
    ideal_next_packet_send_time_(QuicTime::Zero()){
  printf("pcc\n");
}


PCCSender::~PCCSender() {}

void PCCSender::SetFromConfig(const QuicConfig& config,
                                 Perspective perspective) {
}

void PCCSender::ResumeConnectionState(
    const CachedNetworkParameters& cached_network_params,
    bool max_bandwidth_resumption) {
}

void PCCSender::SetNumEmulatedConnections(int num_connections) {

}

void PCCSender::SetMaxCongestionWindow(
    QuicByteCount max_congestion_window) {
}

bool PCCSender::OnPacketSent(
    QuicTime sent_time,
    QuicByteCount bytes_in_flight,
    QuicPacketNumber packet_number,
    QuicByteCount bytes,
    HasRetransmittableData has_retransmittable_data) {

  // TODO : case for retransmission
  if (current_monitor_end_time_.Subtract(QuicTime::Zero()).IsZero()) {
    StartMonitor(sent_time);
    monitors_[current_monitor_].start_seq_num = packet_number;
  } else {
    QuicTime::Delta diff = sent_time.Subtract(current_monitor_end_time_);
    if (diff.ToMicroseconds() > 0) {
      monitors_[current_monitor_].state = WAITING;
      monitors_[current_monitor_].end_transmission_time = sent_time;
      monitors_[current_monitor_].end_seq_num = packet_number;
      current_monitor_end_time_ = QuicTime::Zero();
    }
  }
  PacketInfo packet_info;
  packet_info.sent_time = sent_time;
  packet_info.bytes = bytes;
  monitors_[current_monitor_].packet_vector.push_back(packet_info);
  QuicTime::Delta delay = QuicTime::Delta::FromMicroseconds(
    bytes * 8  * base::Time::kMicrosecondsPerSecond / pcc_utility_.GetCurrentRate() / 1024 / 1024);
  ideal_next_packet_send_time_ = sent_time.Add(delay);
  QuicTime::Delta time = sent_time.Subtract(QuicTime::Zero());
  printf("sent time: %ld\n", time.ToMicroseconds());
  printf("sent at rate %f\n", pcc_utility_.GetCurrentRate());
  printf("sent sequence_number%lu\n\n", packet_number);
  return true;
}

void PCCSender::StartMonitor(QuicTime sent_time){
  current_monitor_ = (current_monitor_ + 1) % NUM_MONITOR;
  pcc_utility_.OnMonitorStart(current_monitor_);

  // calculate monitor interval and monitor end time
  double rand_factor = double(rand() % 3) / 10;
  int64 srtt = rtt_stats_->latest_rtt().ToMicroseconds();
  if (srtt == 0) {
    srtt = rtt_stats_->initial_rtt_us();
  }
  QuicTime::Delta monitor_interval =
      QuicTime::Delta::FromMicroseconds(srtt * (1.5 + rand_factor));
  printf("rtt %ld\n\n", srtt);
  current_monitor_end_time_ = sent_time.Add(monitor_interval);

  monitors_[current_monitor_].state = SENDING;
  monitors_[current_monitor_].start_time = sent_time;
  monitors_[current_monitor_].end_time = QuicTime::Zero();
  monitors_[current_monitor_].end_transmission_time = QuicTime::Zero();
  printf("StartMonitor monitor_num %u\n", current_monitor_);

}

void PCCSender::OnCongestionEvent(
    bool rtt_updated,
    QuicByteCount bytes_in_flight,
    const CongestionVector& acked_packets,
    const CongestionVector& lost_packets) {
  for (CongestionVector::const_iterator it = lost_packets.cbegin();
      it != lost_packets.cend(); ++it) {
    MonitorNumber monitor_num = GetMonitor(it->first);
    if (monitor_num == -1) {
      continue;
    }
    int pos = it->first - monitors_[monitor_num].start_seq_num;
    monitors_[monitor_num].packet_vector[pos].state = LOST;

    if (it->first == monitors_[monitor_num].end_seq_num) {
      EndMonitor(monitor_num);
    }

    printf("lost sequence_number%lu\n\n", it->first);
  }

  for (CongestionVector::const_iterator it = acked_packets.cbegin();
      it != acked_packets.cend(); ++it) {

    MonitorNumber monitor_num = GetMonitor(it->first);
    if (monitor_num == -1) {
      continue;
    }
    int pos = it->first - monitors_[monitor_num].start_seq_num;
    monitors_[monitor_num].packet_vector[pos].state = ACK;

    if (it->first == monitors_[monitor_num].end_seq_num) {
      EndMonitor(monitor_num);
    }
  }
}

void PCCSender::EndMonitor(MonitorNumber monitor_num) {
  printf("EndMonitor monitor_num %u\n", monitor_num);
  if (monitors_[monitor_num].state == WAITING){
    monitors_[monitor_num].state = FINISHED;
    pcc_utility_.OnMonitorEnd(monitors_[monitor_num],
                              rtt_stats_, current_monitor_,
                              monitor_num);
    printf("EndMonitor\n");
  }
}

MonitorNumber PCCSender::GetMonitor(QuicPacketNumber sequence_number) {
  printf("ack sequence_number%lu\n\n", sequence_number);
  MonitorNumber result = current_monitor_;

  do {
    int diff = sequence_number - monitors_[result].start_seq_num;
    if (diff >= 0 && diff < (int)monitors_[result].packet_vector.size()) {
      return result;
    }

    result = (result + 99) % NUM_MONITOR;
  } while (result != current_monitor_);

  printf("%lu\n", sequence_number);
  printf("Monitor is not found\n");
  return -1;
}

void PCCSender::OnRetransmissionTimeout(bool packets_retransmitted) {

}

QuicTime::Delta PCCSender::TimeUntilSend(
      QuicTime now,
      QuicByteCount bytes_in_flight,
      HasRetransmittableData has_retransmittable_data) const {
    // If the next send time is within the alarm granularity, send immediately.
  if (ideal_next_packet_send_time_ > now.Add(alarm_granularity_)) {
    DVLOG(1) << "Delaying packet: "
             << ideal_next_packet_send_time_.Subtract(now).ToMicroseconds();
    return ideal_next_packet_send_time_.Subtract(now);
  }

  DVLOG(1) << "Sending packet now";
  return QuicTime::Delta::Zero();
}

QuicBandwidth PCCSender::PacingRate() const {
  return QuicBandwidth::Zero();
}

QuicBandwidth PCCSender::BandwidthEstimate() const {
  return QuicBandwidth::Zero();
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
  : current_rate_(10),
    previous_utility_(-10000),
    previous_rtt_(0),
    if_starting_phase_(true),
    previous_monitor_(-1),
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
    for (int i = 0; i < NUM_MONITOR; i++) {
      start_rate_array[i] = 0;
    }
    printf("pcc_utility\n");
  }

void PCCUtility::OnMonitorStart(MonitorNumber current_monitor) {
  if (if_starting_phase_) {
    current_rate_ *= 2;
    start_rate_array[current_monitor] = current_rate_;

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

        guess_stat_bucket[i].rate = current_rate_
            + rand_dir * continous_guess_count_ * GRANULARITY * current_rate_;
        guess_stat_bucket[i + 1].rate = current_rate_
            - rand_dir * continous_guess_count_ * GRANULARITY * current_rate_;
        printf("guess at rate %f\n", guess_stat_bucket[i].rate);

      }

    printf("guess monitor_num %u\n", current_monitor);
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

void PCCUtility::OnMonitorEnd(PCCMonitor pcc_monitor,
                              const RttStats* rtt_stats,
                              MonitorNumber current_monitor,
                              MonitorNumber end_monitor) {

  double total = 0;
  double loss = 0;
  GetBytesSum(pcc_monitor.packet_vector, total, loss);

  int64 time =
      pcc_monitor.end_transmission_time.Subtract(pcc_monitor.start_time).ToMicroseconds();

  int64 srtt = rtt_stats->latest_rtt().ToMicroseconds();
  if (previous_rtt_ == 0) previous_rtt_ = srtt;

  double current_utility = ((total-loss)/time*(1-1/(1+exp(-1000*(loss/total-0.05))))
      * (1-1/(1+exp(-80*(1-previous_rtt_/srtt)))) - 1*loss/time) / 1*1000;

  printf("loss %f\n", loss);
  printf("total %f\n", total);


  previous_rtt_ = srtt;

  if (end_monitor == 0 && if_starting_phase_) current_utility /= 2;

  if (if_starting_phase_) {
    if (end_monitor - previous_monitor_ > 1) {
        if_starting_phase_ = false;
        if_make_guess_ = true;
      if (previous_monitor_ == -1) {
        current_rate_ = start_rate_array[0];
      } else {
        current_rate_ = start_rate_array[previous_monitor_];
      }
      return;
    }

    if (previous_utility_ < current_utility) {
      previous_utility_ = current_utility;
      previous_monitor_ = end_monitor;
      return;
    } else {
      if_starting_phase_ = false;
      if_make_guess_ = true;
      current_rate_ = start_rate_array[previous_monitor_];
      return;
    }
  }

  if (if_recording_guess_) {
    // find corresponding monitor
    for (int i = 0; i < NUMBER_OF_PROBE; i++) {
      if (end_monitor == guess_stat_bucket[i].monitor) {
        num_recorded_++;
        guess_stat_bucket[i].utility = current_utility;
      }
    }

    printf("num_recorded_%d\n", num_recorded_);
    if (num_recorded_ == NUMBER_OF_PROBE) {
      num_recorded_ = 0;
      int decision = 0;

      for (int i = 0; i < NUMBER_OF_PROBE; i += 2) {
        bool case1 = guess_stat_bucket[i].utility > guess_stat_bucket[i+1].utility
            && guess_stat_bucket[i].rate > guess_stat_bucket[i+1].rate;
        bool case2 = guess_stat_bucket[i].utility < guess_stat_bucket[i+1].utility
            && guess_stat_bucket[i].rate < guess_stat_bucket[i+1].rate;
      printf("rate1 %f\n", guess_stat_bucket[i].rate);
      printf("utility1 %f\n", guess_stat_bucket[i].utility);
      printf("rate2 %f\n", guess_stat_bucket[i+1].rate);
      printf("utility2 %f\n", guess_stat_bucket[i+1].utility);

        if (case1 || case2) {
          decision += 1;
        } else {
          decision -= 1;
        }
      }

      if_recording_guess_ = false;
      if (decision == 0) {
        if_make_guess_ = true;
      } else {
        change_direction_ = decision > 0 ? 1 : -1;
        printf("change direction %d\n", change_direction_);
        change_intense_ = 1;
        double change_amount = (continous_guess_count_/2+1)
            * change_intense_ * change_direction_ * GRANULARITY * current_rate_;
        current_rate_ += change_amount;

        previous_utility_ = 0;
        continous_guess_count_ = 0;
        tartger_monitor_ = (current_monitor + 1) % NUM_MONITOR;
        printf("tartget monitor_num %u\n", tartger_monitor_);
        if_initial_moving_phase_ = true;
      }
      return;
    }
  }

    // moving phase
    printf("tartget monitor_num %u\n", tartger_monitor_);
    printf("end monitor_num %u\n", end_monitor);
    if (end_monitor == tartger_monitor_) {
      double tmp_rate = total * 8 / time;
      printf("current rate %f\n", current_rate_);
      printf("tmp rate %f\n", tmp_rate);
      if (current_rate_ - tmp_rate > 10 && current_rate_ > 200) {
        current_rate_ = tmp_rate;

        if_make_guess_ = true;
        if_moving_phase_ = false;
        if_initial_moving_phase_ = false;

        change_direction_ = 0;
        change_intense_ = 1;

        guess_time_ = 0;
      }

      if (if_initial_moving_phase_ || current_utility > previous_utility_){
        change_intense_ += 1;
        tartger_monitor_ = (current_monitor + 1) % NUM_MONITOR;
      }

      double change_amount =
          change_intense_ * GRANULARITY * current_rate_ * change_direction_;
      previous_utility_ = current_utility;

      if (if_initial_moving_phase_ || current_utility > previous_utility_){
        current_rate_ += change_amount;
        if_initial_moving_phase_ = false;
      } else {
        current_rate_ -= change_amount;
        if_moving_phase_ = false;
        if_make_guess_ = true;
      }
    }

}

void PCCUtility::GetBytesSum(std::vector<PacketInfo> packet_vector,
                                      double& total,
                                      double& lost) {
  for (std::vector<PacketInfo>::iterator it = packet_vector.begin();
      it != packet_vector.end(); ++it) {
    total += it->bytes;

    if (it->state == LOST){
      lost += it->bytes;
    }
  }
}

double PCCUtility::GetCurrentRate(){
  return current_rate_;
}

}  // namespace net
