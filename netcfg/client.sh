#!/bin/bash
IF_A=$1
IF_D=$2

#address assignment to interface
ip addr add 10.10.1.10/24 dev $IF_A
ip link set $IF_A up
ip addr add 10.10.4.2/30 dev $IF_D
ip link set $IF_D up

#client should not forward packets
sysctl -w net.ipv4.ip_forward=0

#disabling strict reverse path filtering
#packets from server come via LinkD but route back via LAN A
#if rp_filter=1 => kernel drop packet (considering it as spoofing attempt)
sysctl -w net.ipv4.conf.all.rp_filter=0
sysctl -w net.ipv4.conf.default.rp_filter=0

#packets from client to server(lan c) via r1
ip route add 10.10.3.0/24 via 10.10.1.1 dev $IF_A