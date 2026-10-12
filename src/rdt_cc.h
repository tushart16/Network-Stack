#ifndef RDT_CC_H
#define RDT_CC_H

#include <map>
#include <string>
#include <vector>

struct segment_otw {
  unsigned int seq_number;
  std::string payload;
  double time;
};

class RDT_CC {
  public:
    int cwnd, ssthresh;

    unsigned int sent_unacknowledged_seq_number;
    unsigned int send_next_seq_number;
    int receiver_window;
    int duplicate_ack_count;
    double prev_ack_time;

    size_t sent = 0;
    
    unsigned int receive_next_seq_number;
    std::map<unsigned int, std::string> ooo_packets_buffer;
    std::string receive_buffer;
    std::string send_buffer;

    std::vector<segment_otw> segments_otw;

    RDT_CC(unsigned int initial_seq_number);

    void fetch_data(unsigned int seq_number, std::string payload);
    std::vector<std::pair<unsigned int, std::string>> handle_ack(unsigned int ack_number, int window, double curr_time);

    std::vector<std::pair<unsigned int, std::string>> get_segments(int peer_mss, double curr_time);
    std::vector<std::pair<unsigned int, std::string>> get_timeout(double curr_time);

    bool should_disconnect(double curr_time);
};

#endif