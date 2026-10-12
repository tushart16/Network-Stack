#ifndef TCP_H
#define TCP_H

#include <cstdint>
#include <string>

//TCP flags
constexpr uint16_t TCP_FIN = 0x001;
constexpr uint16_t TCP_SYN = 0x002;
constexpr uint16_t TCP_RST = 0x004;
constexpr uint16_t TCP_PSH = 0x008;
constexpr uint16_t TCP_ACK = 0x010;
constexpr uint16_t TCP_URG = 0x020;

struct tcp_header {
  uint16_t src_port;
  uint16_t dest_port;
  uint32_t seq_number;
  uint32_t ack_number;
  uint8_t offset; //4 bits data offset | 4 bits reserved
  uint8_t flags; //cwr, ece, urg, ack, psh, rst, syn, fin
  uint16_t receiver_window;
  uint16_t checksum;
  uint16_t urgent_pointer;
} __attribute__((packed)); //to strip away memory padding

struct checksum_help {
  uint32_t src_address;
  uint32_t dest_address;
  uint8_t zero;
  uint8_t protocol;
  uint16_t length;
}__attribute__((packed));


int build_tcp_header(char* packet, char* src_ip, char* dest_ip, int src_port, int dest_port, 
                      unsigned int seq_number, unsigned int ack_number, int flags, int window, 
                      char* payload, int payload_length);

bool verify_tcp_checksum(char *packet, int tcp_len, char *src_ip, char *dest_ip);

int get_mss(char* packet);

#endif