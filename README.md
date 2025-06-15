# IP over HTTP Tunnel

This project demonstrates how to tunnel IP packets over HTTP between two Linux network namespaces (`ns_client` / `ns_server`), simulating VPN behavior.  
Raw IP packets captured by a TUN interface are encapsulated into HTTP requests/responses for forwarding.

---

## Directory Structure

├── Makefile
├── README.md
├── client_app
├── server_app
├── scripts/
│ └── run.sh
├── include/
│ └── http_client.h
│ └── http_server.h
│ └── injector.h
│ └── tun.h
└── src/
    ├── client.c
    ├── http_client.c
    ├── http_server.c
    ├── injector.c
    ├── server.c
    └── tun.c

- **Makefile**：Build rules for one-step compilation
- **README.md**：Project documentation  
- **include/**：Common header files 
- **src/**：C source code  
- **scripts/run.sh**：One-step cleanup, build, configuration, and launch script  

---

## Environment Dependencies

- **OS**：Linux (kernel ≥ 3.8)
- **Toolchain**：`gcc`, `make`, `bash`  
- **Libraries**：`libcurl`, `libmicrohttpd`, `pthread`  
- **Network Tools**：`iproute2` (`ip`), `curl`, `tcpdump`  

---

## Installation (Ubuntu example)

``` bash
sudo apt update
sudo apt install -y build-essential iproute2 curl \
    libcurl4-openssl-dev libmicrohttpd-dev
sudo modprobe tun
```

---

## Quick Start

``` bash
sudo ./scripts/run.sh all       # clean + build + setup + launch

sudo ./scripts/run.sh clean     # remove old namespaces, veth, processes

sudo ./scripts/run.sh build     # compile server_app and client_app

sudo ./scripts/run.sh setup     # create & configure ns_client/ns_server, veth, TUN, routes

sudo ./scripts/run.sh server    # launch server_app in ns_server

sudo ./scripts/run.sh client    # launch client_app in ns_client

sudo ./scripts/run.sh test      # ping 10.0.200.1 inside ns_client to verify tunnel
```

---

## Architecture Overview

- **TUN Capture**：
    - server_app and client_app each create a TUN device (tun0).
    - System routes for 10.0.100.0/24 or 10.0.200.0/24 point to tun0.

- **HTTP Control Plane**：
    - Each side runs an HTTP server exposing two endpoints:
        - POST /send to receive peer’s IP packets
        - GET /poll to deliver queued IP packets

- **Data Flow**：
    - Server → Client:
        - reader_to_client thread reads IP packets from tun0.
        - Encapsulates and POSTs them to Client’s /send.
    - Client → Server (symmetric):
        - Main loop reads from tun0, POSTs to Server’s /send.
        - injector_from_server thread GETs from Server’s /poll and writes into tun0.

- **Bidirectional Tunnel**：
    - Application-level IP packets are tunneled over HTTP in both directions, achieving VPN-like connectivity.