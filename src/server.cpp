#include "connection.h"
#include "ip.h"
#include "utils.h"
#include "tcp.h"
#include <algorithm>
#include <arpa/inet.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>
#include <fcntl.h>

std::string content_type(std::string path) {
  int last = path.find_last_of('.');
  if(last == std::string::npos) {
    return "application/octet-stream";
  }

  std::string extension = path.substr(last);
  for(auto&c : extension) {c = tolower(c);}

  if (extension == ".html" || extension == ".htm") {
    return "text/html";
  } else if (extension == ".txt") {
    return "text/plain";
  } else if (extension == ".css") {
    return "text/css";
  } else if (extension == ".js") {
    return "application/javascript";
  } else if (extension == ".json") {
    return "application/json";
  } else if (extension == ".png") {
    return "image/png";
  } else if (extension == ".jpg" || extension == ".jpeg") {
    return "image/jpeg";
  } else if (extension == ".gif") {
    return "image/gif";
  } else if (extension == ".pdf") {
    return "application/pdf";
  } 

  return "application/octet-stream";
}

int main(int argc, char* argv[]) {
  int port = 8080;
  std::string docroot = "./www";

  for(int i = 1; i<argc; i++) {
    if(argv[i] == std::string("-p") && i+1<argc) {
      port = std::stoi(argv[++i]);
    } else if(argv[i] == std::string("-d") && i+1<argc) {
      docroot = argv[++i];
    } else if(argv[i] == std::string("--log") && i+1<argc) {
      log_file = argv[++i];
    }
  }

  int sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
  int opt = 1;
  setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &opt, sizeof(opt));
  fcntl(sock, F_SETFL, O_NONBLOCK);
  int receive_buff = 4*1024*1024;
  if(setsockopt(sock, SOL_SOCKET, SO_RCVBUFFORCE, &receive_buff, sizeof(receive_buff)) < 0) {
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &receive_buff, sizeof(receive_buff));
  }

  while(true) {
    char buffer[4096];
    int len_data = recvfrom(sock, buffer, 4096, 0, NULL, NULL);
    if(len_data < 0) {
      usleep(1000);
      continue;
    }
    if(len_data<20) {continue;}

    ip_header* header = (ip_header*) buffer;
    if(!verify_ip_checksum(buffer)) {
      continue;
    }

    int datagram_len = ntohs(header->length);
    if(len_data < ntohs(header->length)) {
      continue;
    }

    int header_len = (header->version_ihl&0x0F)*4;
    if(header_len<20 || len_data<header_len+20) {
      continue;
    }

    tcp_header* header2 = (tcp_header*)(buffer+header_len);

    if(ntohs(header2->dest_port) != port || ((header2->flags)&(TCP_SYN|TCP_ACK|TCP_FIN|TCP_RST)) != TCP_SYN) {
      continue;
    }

    auto get_ip = [] (unsigned int ip) {
      unsigned char* b = (unsigned char*)&ip;
      return std::to_string(b[0]) + "." + std::to_string(b[1]) + "." + std::to_string(b[2]) + "." + std::to_string(b[3]);
    };

    std::string server_ip = get_ip(header->dest_addr);
    std::string client_ip = get_ip(header->src_addr);
    int client_port = ntohs(header2->src_port);

    if(!verify_tcp_checksum((char*)(header2), datagram_len-header_len, (char*)(client_ip.c_str()), (char*)(server_ip.c_str()))) {
      continue;
    }

    Connection c(server_ip, port, client_ip, client_port);

    c.rdt_cc->receive_next_seq_number = ntohl(header2->seq_number)+1;

    int mss_syn = get_mss((char*)header2);
    log_packet("RECV", client_ip, client_port, server_ip, port, header->ttl, ntohs(header->identification),
                ntohl(header2->seq_number), ntohl(header2->ack_number), header2->flags, ntohs(header2->receiver_window),
                datagram_len-header_len-((header2->offset>>4)*4), mss_syn);

    if(mss_syn > 0) {
      c.peer_mss = mss_syn;
    }

    c.send_tcp_seg(0x12, "", true); //syn-ack
    c.rdt_cc->send_next_seq_number++;
    c.state = 4; //syn_received

    c.is_server = true;
    c.connection_fsm();

    std::string req = c.rdt_cc->receive_buffer;
    
    if(req.find("\r\n\r\n") == std::string::npos) {
      if(c.state == 2) {
        c.teardown = 1;
        c.is_server = false;
        c.connection_fsm();
      }
      continue;
    }

    if(c.state != 2) {continue;}

    req = req.substr(0,req.find("\r\n\r\n"));
    std::string req_line = req.substr(0,req.find("\r\n"));
    int spaces = std::count(begin(req_line),end(req_line),' ');

    std::stringstream ss(req_line);
    std::string method, target, version, extra;
    ss>>method>>target>>version>>extra;

    std::string status = "200 OK", body = "", path = "";
    unsigned long content_len = 0;

    if(method.empty() || target.empty() || version.empty() || !extra.empty() || spaces != 2 || target[0] != '/' || 
      !(version.size() == 8 && version.compare(0,5,"HTTP/")==0 && isdigit(version[5]) && isdigit(version[7]) && version[6]=='.')) {
      status = "400 Bad Request";
    } else if(method != "GET" && method != "HEAD") {
      status = "501 Not Implemented";
    } else {
      int idx = target.find("?");
      if(idx != std::string::npos) {
        target = target.substr(0,idx);
      }

      std::stringstream ss(target);
      std::string seg;
      bool dotdot = false;
      while(std::getline(ss,seg,'/')) {
        if(seg == "..") {
          dotdot = true;
          break;
        }
      }

      if(dotdot) {
        status = "403 Forbidden";
      } else {
        path = docroot+target;
        
        std::error_code e;
        if(target.back() == '/' || std::filesystem::is_directory(path,e)) {
          if(path.back() != '/') {
            path += '/';
          }
          path += "index.html";
        }
  
        std::ifstream file(path, std::ios::binary|std::ios::ate);
  
        if(!file) {
          status = "404 Not Found";
        } else {
          std::streamsize sz = file.tellg();
          file.seekg(0,std::ios::beg);

          body.resize(sz);
          file.read(body.data(),sz);
          content_len = sz;

          if(method == "HEAD") {
            body.clear();
          }
        }
      }
    }

    if(status != "200 OK") {
      body = "<html><body><h1>" + status + "</h1></body></html>";
      content_len = body.length();
      if(method == "HEAD") {
        body.clear();
      }
    }

    std::string headers = "HTTP/1.1 " + status + "\r\n" +
                          "Content-Length: " + std::to_string(content_len) + "\r\n" +
                          "Content-Type: " + ((status == "200 OK") ? content_type(path) : "text/html") + "\r\n" +
                          "Connection: close\r\n\r\n";

    c.rdt_cc->send_buffer += (headers + body);
    c.teardown = 1;
    c.is_server = false;
    c.rdt_cc->prev_ack_time = c.get_time();
    c.connection_fsm();
  }
}