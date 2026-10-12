#!/bin/bash
IF_C=$1

#address assignment to interface
ip addr flush dev $IF_C
ip addr add 10.10.3.10/24 dev $IF_C
ip link set $IF_C up

#server should not forward packets
sysctl -w net.ipv4.ip_forward=0
#rp_filter stays strictly 1 by default
sysctl -w net.ipv4.conf.all.rp_filter=1
sysctl -w net.ipv4.conf.default.rp_filter=1
sysctl -w net.ipv4.conf.$IF_C.rp_filter=1

#routing packets back via r2
ip route add default via 10.10.3.1 dev $IF_C
