#!/bin/bash
IF_C=$1

#address assignment to interface
ip addr add 10.10.3.10/24 dev $IF_C
ip link set $IF_C up

#server should not forward packets
sysctl -w net.ipv4.ip_forward=0
#rp_filter stays strictly 1 by default

#routing packets back to client via r2
ip route add 10.10.1.0/24 via 10.10.3.1 dev $IF_C