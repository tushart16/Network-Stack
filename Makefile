CXX = g++
CXXFLAGS = -O2 -Wall -std=c++17

all: src/client src/server

src/client: src/client.cpp src/connection.cpp src/ip.cpp src/tcp.cpp src/rdt_cc.cpp src/utils.cpp
	$(CXX) $(CXXFLAGS) -o src/client $^

src/server: src/server.cpp src/connection.cpp src/ip.cpp src/tcp.cpp src/rdt_cc.cpp src/utils.cpp
	$(CXX) $(CXXFLAGS) -o src/server $^

clean:
	rm -f src/client src/server