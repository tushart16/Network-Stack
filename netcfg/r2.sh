#!/bin/bash
IF_B=$1
IF_C=$2
IF_D=$3

#address assignment to interface
ip addr add 10.10.2.2/24 dev $IF_B
ip link set $IF_B up
ip addr add 10.10.3.1/24 dev $IF_C
ip link set $IF_C up
ip addr add 10.10.4.1/30 dev $IF_D
ip link set $IF_D up

#router should forward packets
sysctl -w net.ipv4.ip_forward=1

#disabling strict reverse path filtering
#packets for client->server on LAN B but server->client on Link D
sysctl -w net.ipv4.conf.all.rp_filter=0
sysctl -w net.ipv4.conf.default.rp_filter=0

#route to reach LAN A via r1
ip route add 10.10.1.0/24 via 10.10.2.1 dev $IF_B

#server->client packets via Link D
#/32 subnet mask used to specify single host ip rather than network.
ip route add 10.10.1.10/32 via 10.10.4.2 dev $IF_D