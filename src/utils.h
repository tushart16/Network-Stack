#ifndef UTILS_H
#define UTILS_H

#include <cstdint>
#include <string>
#include <iostream>
#include <iomanip>

extern std::string log_file;

uint16_t get_checksum(void* temp_data, int len);

void log_packet(std::string dir, std::string src_ip, int src_port,
                std::string dest_ip, int dest_port, int ttl, int id,
                unsigned int seq, unsigned int ack, unsigned int flags, int win, int len,
                int mss);

#endif