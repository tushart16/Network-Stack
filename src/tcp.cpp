#include "tcp.h"
#include "utils.h"
#include <arpa/inet.h>
#include <cstring>
#include <algorithm>

//TCP flags
uint16_t TCP_FIN = 0x001;
uint16_t TCP_SYN = 0x002;
uint16_t TCP_RST = 0x004;
uint16_t TCP_PSH = 0x008;
uint16_t TCP_ACK = 0x010;
uint16_t TCP_URG = 0x020;


int build_tcp_header(char* packet, char* src_ip, char* dest_ip, int src_port, int dest_port, 
                      unsigned int seq_number, unsigned int ack_number, int flags, int window, 
                      char* payload, int payload_length) {
  
  tcp_header* header = (tcp_header*) packet;

  header->src_port = htons(src_port);
  header->dest_port = htons(dest_port);

  header->seq_number = htonl(seq_number);
  header->ack_number = htonl(ack_number);

  uint16_t header_length = sizeof(tcp_header) + ((flags&2) ? 4 : 0); //syn flag

  header->offset = (header_length/4)<<4;
  header->flags = flags;
  header->receiver_window = htons(window);
  header->checksum = 0;
  header->urgent_pointer = 0;

  if((flags&2)) {
    uint8_t* options = (uint8_t*) (packet + sizeof(tcp_header));
    options[0] = 2; //option kind : mss
    options[1] = 4; //option length : 4 bytes

    uint16_t mss = htons(1460);

    std::memcpy(options+2,&mss, sizeof(mss));
  }

  if(payload && payload_length > 0) {
    std::memcpy(packet+header_length,payload,payload_length);
  }

  uint16_t tcp_length = header_length + payload_length;

  checksum_help h2 {};
  h2.src_address = inet_addr(src_ip);
  h2.dest_address = inet_addr(dest_ip);
  h2.zero = 0;
  h2.protocol = IPPROTO_TCP;
  h2.length = htons(tcp_length);

  int temp_size = sizeof(checksum_help) + tcp_length;
  char* temp_seg = new char[temp_size];
  
  std::memcpy(temp_seg,&h2,sizeof(checksum_help));
  std::memcpy(temp_seg+sizeof(checksum_help),packet,tcp_length);

  header->checksum = get_checksum(temp_seg, temp_size);
  delete[] temp_seg;

  return tcp_length;
}

int payload_size(int left_data_len, int peer_mss) {
  return std::min({left_data_len,1460,peer_mss});
}