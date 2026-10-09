#include "rdt_cc.h"
#include <algorithm>
#include <string>
#include <vector>

RDT_CC::RDT_CC(unsigned int intial_seq_number) {
  sent_unacknowledged_seq_number = intial_seq_number + 1;
  send_next_seq_number = intial_seq_number + 1;
  receive_next_seq_number = 0;

  cwnd = 1460;
  ssthresh = 65535;

  receiver_window = 65535;
  duplicate_ack_count = 0;
  prev_ack_time = 0.0;
}

void RDT_CC::fetch_data(unsigned int seq_number, std::string payload) {
  if(payload.empty()) {return;}
  int diff = (int)(seq_number - receive_next_seq_number);
  if(diff == 0) {
    receive_buffer += payload;
    receive_next_seq_number += payload.length();

    while(ooo_packets_buffer.count(receive_next_seq_number)) {
      std::string temp_payload = ooo_packets_buffer[receive_next_seq_number];
      ooo_packets_buffer.erase(receive_next_seq_number);
      receive_buffer += temp_payload;
      receive_next_seq_number += temp_payload.length();
    }
  } else if(diff > 0) {
    ooo_packets_buffer[seq_number] = payload;
  }
}

std::vector<std::pair<unsigned int, std::string>> RDT_CC::handle_ack(unsigned int ack_number, int window, double curr_time) {
  receiver_window = window;
  std::vector<std::pair<unsigned int, std::string>> fast_retransmit;

  int diff = (int)(ack_number - sent_unacknowledged_seq_number);
  if(diff > 0) {
    sent_unacknowledged_seq_number = ack_number;
    prev_ack_time = curr_time;
    duplicate_ack_count = 0;

    std::vector<segment_otw> still_otw;
    for(auto&s : segments_otw) {
      if ((int)(s.seq_number + s.payload.size() - ack_number) > 0) {
        still_otw.emplace_back(s);
      }
    }

    segments_otw = still_otw;

    if(cwnd<ssthresh) {
      cwnd += 1460;
    } else {
      cwnd += (1460*1460)/cwnd;
    }
  } else if (diff == 0) {
    duplicate_ack_count++;
    if(duplicate_ack_count == 3) {
      ssthresh = std::max(cwnd/2,1460);
      cwnd = ssthresh + 3 * 1460;
      if (!segments_otw.empty()) { // fast retransmit
        segments_otw[0].time = curr_time;
        fast_retransmit.push_back({segments_otw[0].seq_number, segments_otw[0].payload});
      }
    }
  }
  return fast_retransmit;
}

std::vector<std::pair<unsigned int, std::string>> RDT_CC::get_segments(int peer_mss, double curr_time) {
  std::vector<std::pair<unsigned int, std::string>> seg;
  int unack_sent_bytes = send_next_seq_number - sent_unacknowledged_seq_number;
  int can_send = std::min(cwnd,receiver_window)-unack_sent_bytes;

  while(can_send>0 && send_buffer.length()) {
    int to_send = std::min({can_send,peer_mss,1460,(int)send_buffer.length()});
    std::string temp_payload = send_buffer.substr(0,to_send);
    send_buffer.erase(0,to_send);

    segment_otw s;
    s.seq_number = send_next_seq_number;
    s.payload = temp_payload;
    s.time = curr_time;
    segments_otw.emplace_back(s);
    seg.push_back({send_next_seq_number, temp_payload});
    send_next_seq_number += temp_payload.size();
    can_send -= temp_payload.size();
  }

  return seg;
}

std::vector<std::pair<unsigned int, std::string>> RDT_CC::get_timeout(double curr_time) {
  std::vector<std::pair<unsigned int, std::string>> seg;
  if(!segments_otw.size()) {
    return seg;
  }

  if(curr_time - segments_otw[0].time >= 1.0) {
    ssthresh = std::max(cwnd/2,1460);
    cwnd = 1460;
    segments_otw[0].time = curr_time;
    seg.push_back({segments_otw[0].seq_number, segments_otw[0].payload});
  }

  return seg;
}

bool RDT_CC::should_disconnect(double curr_time) {
  return (prev_ack_time>0.0 && curr_time-prev_ack_time>=10.0);
}