#!/bin/bash
IF_A=$1
IF_B=$2

#address assignment to interface
ip addr add 10.10.1.1/24 dev $IF_A
ip link set $IF_A up
ip addr add 10.10.2.1/24 dev $IF_D
ip link set $IF_B up

#router should forward packets
sysctl -w net.ipv4.ip_forward=1
#rp_filter stays strictly 1 by default

ip route add default via 10.10.2.2 dev $IF_B