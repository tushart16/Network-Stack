#ifndef IP_H
#define IP_H

#include <cstdint>
#include <cstring>

struct ip_header {
  uint8_t version_ihl; //4 bits each version and internet header length
  uint8_t type_of_service;
  uint16_t length;
  uint16_t identification;
  uint16_t fragment_offset;
  uint8_t ttl;
  uint8_t protocol;
  uint16_t checksum;
  uint32_t src_addr;
  uint32_t dest_addr;
} __attribute__((packed)); //to strip away memory padding

void build_ip_header(char *packet, char *src_ip, char *dest_ip,
                     uint16_t tcp_len, uint16_t id);

bool verify_ip_checksum(char *packet);



#endif