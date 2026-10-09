#include "tcp.h"
#include "ip.h"
#include "utils.h"
#include <arpa/inet.h>
#include <cstdint>
#include <cstring>
#include <algorithm>

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

bool verify_tcp_checksum(char *packet, int tcp_len, char *src_ip, char *dest_ip) {
  tcp_header* header = (tcp_header*) packet;
  uint16_t orig_checksum = header->checksum;
  header->checksum = 0;

  checksum_help h2 {};
  h2.src_address = inet_addr(src_ip);
  h2.dest_address = inet_addr(dest_ip);
  h2.zero = 0;
  h2.protocol = IPPROTO_TCP;
  h2.length = htons(tcp_len);

  int temp_size = sizeof(checksum_help) + tcp_len;
  char* temp_seg = new char[temp_size];
  
  std::memcpy(temp_seg, &h2, sizeof(checksum_help));
  std::memcpy(temp_seg + sizeof(checksum_help), packet, tcp_len);

  uint16_t calculated_checksum = get_checksum(temp_seg, temp_size);
  delete[] temp_seg;

  header->checksum = orig_checksum;
  return orig_checksum == calculated_checksum;
}

int get_mss(char *packet) { // -1 if no mss found
  tcp_header* header = (tcp_header*) packet;
  int header_length = (header->offset >> 4) * 4;
  int curr_offset = sizeof(tcp_header);
  while (curr_offset < header_length) {
    uint8_t kind = *(uint8_t *)(packet + curr_offset);
    if(kind == 0 ){break;} // end of options
    if (kind == 1) { // no-op , ignore it
      curr_offset++;
      continue;
    }
    uint8_t len = *(uint8_t *)(packet + curr_offset + 1);
    if (kind == 2 && len == 4) { // mss option
      uint16_t mss;
      std::memcpy(&mss, packet + curr_offset + 2, 2);
      return ntohs(mss);
    }
    curr_offset += len;
  }
  return -1;
}