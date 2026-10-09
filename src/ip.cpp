#include "ip.h"
#include "utils.h"
#include <arpa/inet.h>
#include <netinet/in.h>

void build_ip_header(char* packet, char* src_ip, char* dest_ip, uint16_t tcp_len, uint16_t id) {
  ip_header* header = (ip_header*) packet;

  header->version_ihl = (4<<4)|5;  //4 bits version ipv4 and 4 bits ihl : 0100|0101
  header->type_of_service = 0;
  header->length = htons(sizeof(ip_header)+tcp_len);
  header->identification = htons((id) ? id : 1);
  header->fragment_offset = 0;
  header->ttl = 64;
  header->protocol = IPPROTO_TCP; //ip protocol no for tcp (6)

  //string ips -> network byte order integers
  header->src_addr = inet_addr(src_ip);
  header->dest_addr = inet_addr(dest_ip);

  header->checksum = 0;
  header->checksum = get_checksum(header, 20);
  //htons and inet_addr are used for little to big endian conversion of bytes ordering
}