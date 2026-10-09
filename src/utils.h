#ifndef UTILS_H
#define UTILS_H

#include <cstdint>
#include <string>
#include <iostream>
#include <iomanip>

uint16_t get_checksum(void* temp_data, int len);

void log_packet(std::string dir, std::string src_ip, int sport,
                std::string dst_ip, int dport, int ttl, int id,
                unsigned int seq, unsigned int ack, int flags, int win, int len,
                int mss);

#endif