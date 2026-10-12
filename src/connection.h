#ifndef CONNECTION_H
#define CONNECTION_H

#include <cstdint>
#include <string>
#include "rdt_cc.h"

class Connection {
  public:
    int sock;
    int state; // 0->closed, 1->syn sent, 2->established, 3->fin_wait, 4->syn received 

    bool is_server = false;
    bool teardown = false;
    bool peer_fin_received = false;
    
    std::string local_ip;
    int local_port;
    std::string remote_ip;
    int remote_port;
    uint16_t packet_id;
    int peer_mss;
    double handshake_start_time;
    double last_syn_time;
    double last_fin_time;
    double last_fin_retransmit;

    RDT_CC* rdt_cc;
    
    Connection(std::string src_ip, int src_port, std::string dest_ip, int dest_port);
    ~Connection();

    void start_connection();
    void initiate_teardown();
    void connection_fsm();
    void send_tcp_seg(int flags, std::string payload, bool include_mss);
    void send_tcp_seg_with_seq(int flags, std::string payload, bool include_mss, unsigned int seq);

    double get_time();
};

#endif