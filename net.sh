#!/bin/sh

# Run this on devel platform (host) startup.
# Modify it to suit your needs.

sudo brctl addbr br0
sudo ifconfig br0 192.168.0.105
sudo ifconfig br0 promisc
sudo ifconfig br0 up
sudo sysctl -w net.ipv4.ip_forward=1
sudo /etc/init.d/isc-dhcp-server restart
#sudo route add -host 10.0.2.3 dev br0
#sudo route add -host 10.0.2.4 dev br0

