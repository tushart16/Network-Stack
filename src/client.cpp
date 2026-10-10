#include "connection.h"
#include <cctype>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <cstdlib>
#include <netdb.h>
#include <arpa/inet.h>
#include <ctime>

bool parse_url(std::string url, std::string &host, std::string &host_header,  int &port, // extract host, host header, port and path from the url
               std::string &path) {
  if (url.find("http://") == 0) {
    url = url.substr(7);
  }
  else {return 0;}
  int path_start_idx = url.find('/');
  if (path_start_idx == std::string::npos) {
    path = '/';
  } else {
    path = url.substr(path_start_idx);
    url = url.substr(0,path_start_idx);// reomve the path part from url
  }
  host_header = url; 
  int port_idx = url.find(':');
    if (port_idx == std::string::npos) {
      port = 80; // default port
      host = url;
    } else {
      try { port = std::stoi(url.substr(port_idx + 1));
      } catch (...) {
        return false;
      }
      if (port < 1 || port > 65535) {
        return false;
      }
      host = url.substr(0, port_idx);
    }
    return !host.empty();
}

int main(int argc, char* argv[]) {
  if (argc < 2) return 2;
  
  std::string url = argv[1];
  std::string outputfile = "" , logfile = "";
  for (int i = 2; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "-o" && i + 1 < argc) {
        outputfile = argv[++i];
    }
    else if (arg == "--log" && i + 1 < argc) { 
      logfile = argv[++i];                     
    }
  }

  std::string host, path, host_header;
  int port;
  if (!parse_url(url, host, host_header, port, path)) {
    return 2;
  }
  
  struct hostent *he = gethostbyname(host.c_str());
  if (he == NULL || he->h_addrtype != AF_INET) return 2; // DNS failure -> exit 2
  std::string server_ip = inet_ntoa(*(struct in_addr*)he->h_addr_list[0]);
  std::string client_ip = "10.10.1.10";

  srand(time(NULL));
  int client_port = 61000 + (rand() % 4535); // random port in this range

  Connection conn(client_ip, client_port, server_ip, port);

  std::string request = "GET " + path + " HTTP/1.1\r\n" + "Host: " + host_header + "\r\n" + "Connection: close\r\n\r\n";
  conn.rdt->send_buffer += request;
  conn.start_connection();
  conn.receive_loop(); // receive stuff untill connection closes

  std::string response = conn.rdt->receive_buffer;

  int header_end = response.find("\r\n\r\n");
  if (header_end == std::string::npos) {
    return 2; //incomplete response
  }
  std::string body = response.substr(header_end + 4);
  std::string header = response.substr(0, header_end);

  int status_end = header.find("\r\n");
  std::string status = header.substr(0, status_end);
  std::cerr<<status<<"\n";
  if (status.length() < 12) { // malformed status
    return 2;
  }
  int status_code;
  try { status_code = std::stoi(status.substr(9, 3)); }
  catch (...) { return 2; }

  std::string header_lower = header;
  for (auto &c : header_lower) {
    c = tolower(c);
  }

  bool complete = 0;
  if (header_lower.find("transfer-encoding: chunked") != std::string::npos) {
    std::string unchunked_body = "";
    int i = 0;
    while (i < body.length()) {
        int crlf = body.find("\r\n", i); // carriage return + line feed
        if (crlf == std::string::npos){ break;}
        
        std::string len_in_hex = body.substr(i, crlf - i);
        int chunk_len;
        try { chunk_len = std::stoi(len_in_hex, nullptr, 16); } //converted to decimal
        catch (...) { break; }
        if (chunk_len == 0) { complete = true; break; } 
        if (crlf + 2 + chunk_len + 2 > body.length()) {break;} // malformed header
        
        unchunked_body += body.substr(crlf + 2, chunk_len);
        i = crlf + 2 + chunk_len + 2; // skip chunk data and trailing \r\n
    }
    body = unchunked_body;
  } else {
        int content_len_idx = header_lower.find("\r\ncontent-length:");
        if (content_len_idx != std::string::npos) {
            try {
                unsigned long content_len = std::stoul(header_lower.substr(content_len_idx + 17));
                if (body.length() >= content_len) {
                    body = body.substr(0, content_len);
                    complete = true;
                }
            } catch (...) {}
        } else {
            complete = true; // body ends when the server closes the connection
        }
  }

  if (!outputfile.empty()) {
    std::ofstream out(outputfile, std::ios::binary);
    out.write(body.c_str(), body.length());
    out.close();
  } else {
      std::cout << body;
  }

  if (!complete) {
    return 2; // http header incomplete/wrong!
  }
  if (status_code >= 200 && status_code < 300) {
    return 0;
  }
  return 1;
}