#!/bin/sh

# Run this on devel platform (host) startup.
# Modify it to suit your needs.

sudo ip link add br0 type bridge
sudo sysctl -w net.ipv4.ip_forward=1
sudo ip link set br0 up
sudo route add -host 10.0.2.3 dev br0
sudo route add -host 10.0.2.4 dev br0

