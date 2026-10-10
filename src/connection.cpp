#include "connection.h"
#include "ip.h"
#include "tcp.h"
#include "utils.h"
#include <string>
#include <arpa/inet.h>
#include <unistd.h> //close and usleep
#include <fcntl.h> //non blocking sockets
#include <chrono>
#include <random>

Connection::Connection(std::string src_ip, int src_port, std::string dest_ip, int dest_port) {
  local_ip = src_ip;
  local_port = src_port;
  remote_ip = dest_ip;
  remote_port = dest_port;
  state = 0; //closed
  packet_id = 142; //inital id for ip
  peer_mss = 1460;

  sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP); //raw socket (IPv4 TCP)
  int opt = 1; //bool true for socket config (ip header include)
  setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &opt, sizeof(opt)); //to forbit os from injecting its ip header (IP_HDRINCL)
  fcntl(sock, F_SETFL, O_NONBLOCK); //set socket to non-blocking so the loop can manage timeouts
  
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<unsigned int> distrbn;
  unsigned int initial_seq_number = distrbn(gen); // random inital seq no.
  rdt_cc = new RDT_CC(initial_seq_number);
  handshake_start_time = get_time();
  last_syn_time = handshake_start_time;
  rdt_cc->prev_ack_time = handshake_start_time;
}

Connection::~Connection() {
  close(sock);
  delete rdt_cc;
}

double Connection::get_time() {
  return std::chrono::duration<double>((std::chrono::steady_clock::now()).time_since_epoch()).count();
}

void Connection::send_tcp_seg(int flags, std::string payload, bool include_mss) {
  send_tcp_seg_with_seq(flags, payload, include_mss, rdt_cc->send_next_seq_number);
}

void Connection::send_tcp_seg_with_seq(int flags, std::string payload, bool include_mss, unsigned int seq) {
  char datagram[4096];
  std::memset(datagram, 0, 4096);

  int tcp_len = build_tcp_header(datagram + 20, (char*)local_ip.c_str(), (char*)remote_ip.c_str(), 
                                 local_port, remote_port, 
                                 seq, rdt_cc->receive_next_seq_number, 
                                 flags, 65535, (char*)payload.c_str(), payload.length());

  build_ip_header(datagram, (char*)local_ip.c_str(), (char*)remote_ip.c_str(), tcp_len, packet_id++);

  struct sockaddr_in dest;
  dest.sin_family = AF_INET;
  dest.sin_port = htons(remote_port);
  dest.sin_addr.s_addr = inet_addr(remote_ip.c_str());

  sendto(sock, datagram, 20 + tcp_len, 0, (struct sockaddr *)&dest, sizeof(dest));

  log_packet("SEND", local_ip, local_port, remote_ip, remote_port, 64, packet_id - 1, 
          seq, rdt_cc->receive_next_seq_number, flags, 65535, 
          payload.length(), include_mss ? 1460 : -1);
}

void Connection::start_connection() {
  handshake_start_time = get_time();
  last_syn_time = handshake_start_time;
  rdt_cc->prev_ack_time = handshake_start_time; // initialize 10s timer
  send_tcp_seg(TCP_SYN, "", true);
  state = 1; // syn sent
  rdt_cc->send_next_seq_number++; // syn consumes 1 sequence number
}

void Connection::initiate_teardown() {
  if (state == 2) {
    send_tcp_seg(TCP_FIN | TCP_ACK, "", false);
    rdt_cc->send_next_seq_number++; // fin consumes 1 sequence number
    state = 3;
    last_fin_time = get_time();
    last_fin_retransmit = last_fin_time;
  }
}

void Connection::receive_loop() {
  char buffer[4096];

  while (true) {
    usleep(1000);
    double curr_time = get_time();

    if (state == 2 && teardown && rdt_cc->send_buffer.empty() && 
      rdt_cc->sent_unacknowledged_seq_number == rdt_cc->send_next_seq_number) {
      initiate_teardown();
    }
    if(is_server && rdt_cc->receive_buffer.find("\r\n\r\n") != std::string::npos) {
      break;
    }
    
    if (state == 2 && rdt_cc->should_disconnect(curr_time)) {
      state = 0;
      break;
    }
    if (state == 2) {
      auto outgoing_segments = rdt_cc->get_segments(peer_mss, curr_time); // drain send buffer
      for (auto& p : outgoing_segments) {
        send_tcp_seg_with_seq(TCP_ACK, p.second, false, p.first);
      }
    }
    if (state == 1) { // syn sent
      if (curr_time - handshake_start_time >= 6.0) {
        break; // abort after trying for 6s
      }
      if (curr_time - last_syn_time >= 1.0) {
        rdt_cc->send_next_seq_number--;
        send_tcp_seg(TCP_SYN, "", true);
        rdt_cc->send_next_seq_number += 1;
        last_syn_time = curr_time;
      }
    } else if (state == 2) { //est
      auto timeouts = rdt_cc->get_timeout(curr_time);
      for (auto& p : timeouts) {
         send_tcp_seg_with_seq(TCP_ACK, p.second, false,p.first);
      }
    } else if (state == 3) { // fin_wait
      if (curr_time - last_fin_time >= 2.0) {
        break;
      }
      if (curr_time - last_fin_retransmit >= 1.0 && (rdt_cc->sent_unacknowledged_seq_number < rdt_cc->send_next_seq_number)) {
        rdt_cc->send_next_seq_number--;
        send_tcp_seg(TCP_FIN | TCP_ACK, "", false);
        rdt_cc->send_next_seq_number++;
        last_fin_retransmit = curr_time;
      }
    } else if (state == 4) { // syn->rcvd
      if (curr_time - handshake_start_time >= 6.0) {
        break;
      }
      if (curr_time - last_syn_time >= 1.0) {
          rdt_cc->send_next_seq_number--;
          send_tcp_seg(TCP_SYN | TCP_ACK, "", true);
          rdt_cc->send_next_seq_number++;
          last_syn_time = curr_time;
      }
    }
    int data_sz = recvfrom(sock, buffer, 4096, 0, NULL, NULL);
    if (data_sz < 20) {
      continue;
    } // non blocking read
    ip_header *iph = (ip_header *)buffer;
    if (!verify_ip_checksum(buffer)) {
      continue;
    }
    if (data_sz < ntohs(iph->length)) {
      continue;
    }
    if (iph->src_addr != inet_addr(remote_ip.c_str()) || iph->dest_addr != inet_addr(local_ip.c_str())) {
      continue;
    }
    int ip_header_len = (iph->version_ihl & 0x0F) * 4;

    tcp_header *tcph = (tcp_header *)(buffer + ip_header_len);
    int datagram_len = ntohs(iph->length);
    int tcp_header_len = (tcph->offset >> 4) * 4;
    int payload_len = datagram_len - ip_header_len - tcp_header_len;
    if(ntohs(tcph->src_port) != remote_port || ntohs(tcph->dest_port) != local_port){continue;}
    if (!verify_tcp_checksum((char *)tcph, datagram_len - ip_header_len,
                             (char *)remote_ip.c_str(), (char *)local_ip.c_str())) {
      continue;
    }
    int mss_val = get_mss((char *)tcph);
    if (mss_val > 0) {
      peer_mss = mss_val;
    }
    unsigned int recv_seq = ntohl(tcph->seq_number);
    unsigned int recv_ack = ntohl(tcph->ack_number);
    int flags = tcph->flags;
    log_packet("RECV", remote_ip, remote_port, local_ip, local_port, iph->ttl,
               ntohs(iph->identification), recv_seq, recv_ack, flags,
               ntohs(tcph->receiver_window), payload_len, mss_val);
    if (flags & TCP_RST) {
      state = 0; // connection closed
      break;
    }
    if (state == 4 && (flags & TCP_ACK) && !(flags & TCP_SYN)) {
      if (recv_ack == rdt_cc->send_next_seq_number) {
        state = 2;
        rdt_cc->sent_unacknowledged_seq_number = recv_ack;
      }
    }

    if (state == 2 && payload_len > 0) {
      std::string data(buffer + ip_header_len + tcp_header_len, payload_len);
      rdt_cc->fetch_data(recv_seq, data);
      send_tcp_seg(TCP_ACK, "", false); // cumulative ack
    }
    if ((state == 2 || state == 3) && (flags & TCP_ACK)) {
      auto fast_retrans = rdt_cc->handle_ack(recv_ack, ntohs(tcph->receiver_window), curr_time);
      for (auto& p : fast_retrans) {
          send_tcp_seg_with_seq(TCP_ACK, p.second, false, p.first);
      }
      if (state == 3 && peer_fin_received && rdt_cc->sent_unacknowledged_seq_number >= rdt_cc->send_next_seq_number) {
        state = 0;
        break;
      }
    }
    if (state == 2 && (flags & TCP_SYN)) {
      send_tcp_seg(TCP_ACK, "", false);
    }
    if (state == 1 && (flags & TCP_SYN) && (flags & TCP_ACK)) { // syn-ack
      if (recv_ack != rdt_cc->send_next_seq_number) {
        continue;
      }
      rdt_cc->receive_next_seq_number = recv_seq + 1; 
      rdt_cc->sent_unacknowledged_seq_number = recv_ack;
      state = 2;
      send_tcp_seg(TCP_ACK, "", false); // complete handshake
      continue;
    }
    if (flags & TCP_FIN) {
      unsigned int fin_seq = recv_seq + payload_len;
      if (fin_seq == rdt_cc->receive_next_seq_number) { // new, in-order fin
          rdt_cc->receive_next_seq_number++;
          peer_fin_received = true;
          send_tcp_seg(TCP_ACK, "", false);
          
          if (state != 3) { // Send fin if we haven't already
              send_tcp_seg(TCP_FIN | TCP_ACK, "", false);
              rdt_cc->send_next_seq_number++;
              state = 3;
              last_fin_time = curr_time;
              last_fin_retransmit = curr_time;
          }
          else if (rdt_cc->sent_unacknowledged_seq_number >= rdt_cc->send_next_seq_number) {
            // Both sides have fin'd and ack'd. can close immediately.
            state = 0;
            break;
          }
      } else { //out of order / dup FIN
          send_tcp_seg(TCP_ACK, "", false); 
      }
    }
  }
}