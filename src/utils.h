#ifndef UTILS_H
#define UTILS_H
#include <string>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <iomanip>

int get_checksum(unsigned short *buff, int bytes);

void log_packet(std::string *log_file, std::string &dir, std::string &src_ip,
                int src_port, std::string &dst_ip, int dst_port, int ttl,
                int id, unsigned int seq, unsigned int ack, int flags, int win,
                int len, int mss);

struct pseudo_header {
    unsigned int src_address;
    unsigned int dst_address;
    unsigned char placeholder;
    unsigned char protocol;
    unsigned short tcp_length;
};
#endif