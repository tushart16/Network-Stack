#ifndef CONNECTION_H
#define CONNECTION_H

#include <string>

#include "rdt_cc.h"

class Connection {
public:
  int sock;
  int state; // 0->closed, 1->syn sent, 2-> established, 3 ->fin_wait, 4-> syn_rcvd

  bool is_server = false;
  bool teardown = false;
  bool peer_fin_received = false;
  
  std::string local_ip;
  int local_port;
  std::string remote_ip;
  int remote_port;
  int packet_id;
  int peer_mss;
  double handshake_start_time;
  double last_syn_time;
  double last_fin_time;
  double last_fin_retransmit;

  RDT_CC *rdt;
  
  Connection(std::string src_ip, int s_port, std::string dst_ip, int d_port);
  ~Connection();

  void start_connection();
  void initiate_teardown();
  void receive_loop();
  void send_tcp_seg(int flags, std::string payload, bool include_mss);
  void send_tcp_seg_with_seq(int flags, std::string payload, bool include_mss, unsigned int seq);

  double get_time();
};

#endif