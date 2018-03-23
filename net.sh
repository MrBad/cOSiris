#!/bin/sh

# Run this on devel platform (host) startup.
# You need a DHCP server on your host, on IP 192.168.0.105
# Also this assumes all traffic goes on the internet via wlan1
# Modify it to suit your needs.

sudo brctl addbr br0
sudo ifconfig br0 192.168.0.105
sudo ifconfig br0 promisc
sudo ifconfig br0 up
sudo sysctl -w net.ipv4.ip_forward=1
sudo /etc/init.d/isc-dhcp-server restart
#sudo route add -host 10.0.2.3 dev br0
#sudo route add -host 10.0.2.4 dev br0

sudo iptables -t nat -A POSTROUTING -o wlan1 -j MASQUERADE
sudo iptables -A FORWARD -i br0 -o wlan1 -j ACCEPT

