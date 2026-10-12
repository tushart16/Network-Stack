#include "utils.h"
#include <cstdio>
#include <fstream>
#include <string>

std::string log_file = "";

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

void log_packet(std::string dir, std::string src_ip, int src_port,
                std::string dest_ip, int dest_port, int ttl, int id,
                unsigned int seq, unsigned int ack, unsigned int flags, int win, int len,
                int mss) {
  char flag_log[10];
  snprintf(flag_log, sizeof(flag_log), "0x%02x", flags&0xFF);
  std::string log = dir + " " + src_ip + ":" + std::to_string(src_port) + " > " + dest_ip + ":" + std::to_string(dest_port) + 
                    " ttl=" + std::to_string(ttl) + " id=" + std::to_string(id) + " seq=" + std::to_string(seq) + " ack=" + std::to_string(ack) +
                    " flags=" + flag_log + " win=" + std::to_string(win) + " len=" + std::to_string(len) + 
                    " mss=" + ((mss>0) ? std::to_string(mss) : "-") + "\n";

  if(!log_file.empty()) {
    std::ofstream out(log_file,std::ios::app);
    out << log;
  } else {
    // std::cout << log;
    // std::cout.flush();  
  }
}