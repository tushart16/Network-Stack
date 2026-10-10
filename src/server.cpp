#include "connection.h"
#include "ip.h"
#include "tcp.h"
#include <arpa/inet.h>
#include <filesystem>
#include <netinet/in.h>
#include <sstream>
#include <fstream>
#include <string>
#include <sys/socket.h>
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
  std::string root = "./www";

  for(int i = 1; i<argc; i++) {
    if(argv[i] == std::string("-p") && i+1<argc) {
      port = std::stoi(argv[++i]);
    } else if(argv[i] == std::string("-d") && i+1<argc) {
      root = argv[++i];
    } 
  }

  int sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
  fcntl(sock, F_SETFL, O_NONBLOCK);

  while(true) {
    usleep(1000);

    char buffer[4096];
    int len_data = recvfrom(sock, buffer, 4096, 0, NULL, NULL);
    if(len_data<20) {continue;}

    ip_header* header = (ip_header*) buffer;
    if(!verify_ip_checksum(buffer)) {
      continue;
    }

    int header_len = (header->version_ihl&0x0F)*4;
    if(len_data<header_len+20) {
      continue;
    }

    tcp_header* header2 = (tcp_header*)(buffer+header_len);

    if(ntohs(header2->dest_port) || header2->flags != 0x02) {
      continue;
    }

    auto get_ip = [] (unsigned int ip) {
      unsigned char* b = (unsigned char*)&ip;
      return std::to_string(b[0]) + "." + std::to_string(b[1]) + "." + std::to_string(b[2]) + "." + std::to_string(b[3]);
    };

    std::string server_ip = get_ip(header->dest_addr);
    std::string client_ip = get_ip(header->src_addr);
    int client_port = ntohs(header2->src_port);

    Connection c("10.10.3.10", port, client_ip, client_port);

    c.rdt->receive_next_seq_number = ntohl(header2->seq_number)+1;
    c.send_tcp_seg(0x12, "", true); //syn-ack
    c.state = 4; //syn_received

    c.is_server = true;
    c.receive_loop();

    std::string req = c.rdt->receive_buffer;
    if(req.empty()) {continue;}
    if(req.find("\r\n\r\n") == std::string::npos) {continue;}

    req = req.substr(0,req.find("\r\n\r\n"));

    std::stringstream ss(req);
    std::string method, target, version;
    ss>>method>>target>>version;

    std::string status = "200 OK", body = "", path = "";

    if(method.empty() || target.empty() || version.empty() || target[0] != '/' || version.find("HTTP/") != 0) {
      status = "400 Bad Request";
    } else if(method != "GET" || method != "HEAD") {
      status = "501 Not Implemented";
    } else if(target.find("..") != std::string::npos) {
      status = "403 Forbidden";
    } else {
      int idx = target.find("?");
      if(idx != std::string::npos) {
        target = target.substr(0,idx);
      }
      path = root+target;

      if(target.back() == '/' || std::filesystem::is_directory(path)) {
        if(path.back() != '/') {
          path += '/';
        }
        path += "index.html";
      }

      std::ifstream file(path, std::ios::binary);

      if(!file) {
        status = "404 Not Found";
      } else if(method == "GET") {
        std::ostringstream os;
        os << file.rdbuf();
        body = os.str();
      }
    }

    if(status != "200 OK") {
      body = "<html><body><h1>" + status + "</h1></body></html>";
    }

    std::string headers = "HTTP/1.1 " + status + "\r\n" +
                          "Content-Length: " + std::to_string(body.length()) + "\r\n" +
                          "Content-Type: " + content_type(path) + "\r\n" +
                          "Connection: close\r\n\r\n";

    c.rdt->send_buffer += (headers + body);

    c.initiate_teardown();
    c.is_server = false;
    c.receive_loop();
  }
}