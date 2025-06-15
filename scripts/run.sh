#!/usr/bin/env bash
set -euo pipefail

ACTION=${1:-help}

# clean up old namespaces and devices
function clean_ns() {
  pkill -f server_app 2>/dev/null ||:
  pkill -f client_app 2>/dev/null ||:
  sudo ip netns del ns_client 2>/dev/null ||:
  sudo ip netns del ns_server 2>/dev/null ||:
  sudo ip link del veth_c 2>/dev/null ||:
  sudo ip link del veth_s 2>/dev/null ||:
  sudo ip tuntap del dev tun0 mode tun 2>/dev/null ||:
  echo "cleaned old namespaces/devices"
}

# set up network namespaces and veth/tun interfaces
function setup_ns() {
  sudo ip netns add ns_client
  sudo ip netns add ns_server
  sudo ip link add veth_c type veth peer name veth_s
  sudo ip link set veth_c netns ns_client
  sudo ip link set veth_s netns ns_server

  # ns_client
  sudo ip netns exec ns_client ip link set lo up
  sudo ip netns exec ns_client ip link set veth_c up
  sudo ip netns exec ns_client ip addr add 192.168.100.1/24 dev veth_c
  sudo ip netns exec ns_client ip tuntap add dev tun0 mode tun user $(whoami)
  sudo ip netns exec ns_client ip link set tun0 up
  sudo ip netns exec ns_client ip addr add 10.0.100.1/24 dev tun0
  sudo ip netns exec ns_client ip route add 10.0.200.0/24 dev tun0

  # ns_server
  sudo ip netns exec ns_server ip link set lo up
  sudo ip netns exec ns_server ip link set veth_s up
  sudo ip netns exec ns_server ip addr add 192.168.100.2/24 dev veth_s
  sudo ip netns exec ns_server ip tuntap add dev tun0 mode tun user $(whoami)
  sudo ip netns exec ns_server ip link set tun0 up
  sudo ip netns exec ns_server ip addr add 10.0.200.1/24 dev tun0
  sudo ip netns exec ns_server ip route add 10.0.100.0/24 dev tun0

  echo "namespaces & veth/tun configured"
}

# start server_app
function start_server() {
  sudo ip netns exec ns_server ./server_app 8000 192.168.100.1 8000 &
  echo "server_app started in ns_server on port 8000"
}

# start client_app
function start_client() {
  sudo ip netns exec ns_client ./client_app 192.168.100.2 8000 &
  echo "client_app started in ns_client -> server 192.168.100.2:8000"
}

case "$ACTION" in
  clean) clean_ns ;;
  setup) setup_ns ;;
  server) start_server ;;
  client) start_client ;;
  all)
    clean_ns
    setup_ns
    make
    start_server
    start_client
    ;;
  *)
    echo "Usage: $0 {clean|setup|server|client|all}"
    ;;
esac