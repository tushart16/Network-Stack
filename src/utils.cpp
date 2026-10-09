#include "utils.h"
#include <cstdio>
#include <string>

uint16_t get_checksum(void* temp_data, int len) {
  uint16_t* data = (uint16_t*) temp_data;
  uint32_t sum = 0;

  while(len>1) {
    sum += *data++;
    len -= 2;
  }

  if(len==1) {
    uint16_t left = *(uint8_t*) data;
    sum += left; 
  }

  while(sum>>16) {
    sum = (sum&(0xffff))+(sum>>16);
  }

  return (uint16_t)(~sum);
}

void log_packet(std::string dir, std::string src_ip, int sport,
                std::string dst_ip, int dport, int ttl, int id,
                unsigned int seq, unsigned int ack, int flags, int win, int len,
                int mss)
{
  char flag_str[10];
  snprintf(flag_str, sizeof(flag_str), "0x%02x", static_cast<unsigned int>(flags) & 0xFF);
  std::cout << dir << " " << src_ip << ":" << sport << " > " << dst_ip << ":" << dport << " ttl=" << ttl 
            << " id=" << id << " seq=" << seq << " ack=" << ack  << " flags=" << flag_str << " win=" << win 
            << " len=" << len << " mss=" << ((mss>0) ? std::to_string(mss) : "-") << "\n";
  std::cout.flush();  
}