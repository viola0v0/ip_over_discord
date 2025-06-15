# —— 清理旧状态 —— #

# 1) 杀掉可能正在跑的 server_app / client_app
pkill -f server_app 2>/dev/null ||:
pkill -f client_app 2>/dev/null ||:

# 2) 删除旧的网络命名空间
sudo ip netns del ns_client 2>/dev/null ||:
sudo ip netns del ns_server 2>/dev/null ||:

# 3) 删除宿主机上的 veth 设备（如果残留）
#    注意：ip link del 会把 peer 也一起删掉
sudo ip link del veth_c 2>/dev/null ||:
sudo ip link del veth_s 2>/dev/null ||:

# 4) 删除任意残留的 tun0（宿主机命名空间外）
#    （通常没必要，如果你在宿主机上创建过 tun0）
sudo ip tuntap del dev tun0 mode tun 2>/dev/null ||:

# 5) 确保宿主机路由表干净
#    （可选：查看有没有与 ns_client/ns_server 子网冲突的路由）
ip route show | grep -E "192\.168\.100\.|10\.0\.(100|200)\." && \
  echo "请手动删除上面这些路由" 

echo "🧹 清理完毕，请继续执行你的 8 步流程。"

# =========================================
# 1. 安装依赖 & 编译（宿主机一次性完成）
# =========================================
sudo apt update
sudo apt install -y build-essential iproute2 curl libcurl4-openssl-dev libmicrohttpd-dev

# 确保 TUN 驱动加载
sudo modprobe tun

cd ~/GitHub/ip_over_discord

# 编译 Server
gcc -Iinclude -o server_app \
    src/tun.c src/http_server.c src/injector.c src/http_client.c src/server.c \
    -lcurl -lpthread -lmicrohttpd

# 编译 Client
gcc -Iinclude -o client_app \
    src/tun.c src/http_client.c src/http_server.c src/client.c \
    -lcurl -lpthread -lmicrohttpd

# 确认时间戳是刚刚编译的
ls -l server_app client_app

# =========================================
# 2. 创建两个网络命名空间 & veth（宿主机）
# =========================================
sudo ip netns del ns_client 2>/dev/null ||:
sudo ip netns del ns_server 2>/dev/null ||:

sudo ip netns add ns_client
sudo ip netns add ns_server

# 管理链路 veth_c <--> veth_s
sudo ip link add veth_c type veth peer name veth_s
sudo ip link set veth_c netns ns_client
sudo ip link set veth_s netns ns_server

# =========================================
# 3. 配置 ns_client（管理链路 & 隧道）
# =========================================
# 启用 loopback + veth_c
sudo ip netns exec ns_client ip link set lo up
sudo ip netns exec ns_client ip link set veth_c up
sudo ip netns exec ns_client ip addr add 192.168.100.1/24 dev veth_c

# 创建并配置 tun0
sudo ip netns exec ns_client ip tuntap add dev tun0 mode tun user $(whoami)
sudo ip netns exec ns_client ip link set tun0 up
sudo ip netns exec ns_client ip addr add 10.0.100.1/24 dev tun0

# 把所有发往 10.0.200.0/24 的流量通过 tun0
sudo ip netns exec ns_client ip route add 10.0.200.0/24 dev tun0

# =========================================
# 4. 配置 ns_server（管理链路 & 隧道）
# =========================================
# 启用 loopback + veth_s
sudo ip netns exec ns_server ip link set lo up
sudo ip netns exec ns_server ip link set veth_s up
sudo ip netns exec ns_server ip addr add 192.168.100.2/24 dev veth_s

# 创建并配置 tun0
sudo ip netns exec ns_server ip tuntap add dev tun0 mode tun user $(whoami)
sudo ip netns exec ns_server ip link set tun0 up
sudo ip netns exec ns_server ip addr add 10.0.200.1/24 dev tun0

# 把所有发往 10.0.100.0/24 的流量通过 tun0
sudo ip netns exec ns_server ip route add 10.0.100.0/24 dev tun0

# =========================================
# 5. 验证管理链路（veth_c <--> veth_s）通
# =========================================
sudo ip netns exec ns_client ping -c2 192.168.100.2
<!-- --- 192.168.100.2 ping statistics ---
2 packets transmitted, 2 received, 0% packet loss, time 1034ms
rtt min/avg/max/mdev = 0.034/0.107/0.181/0.073 ms -->

sudo ip netns exec ns_server ping -c2 192.168.100.1
<!-- --- 192.168.100.1 ping statistics ---
2 packets transmitted, 2 received, 0% packet loss, time 1060ms
rtt min/avg/max/mdev = 0.028/0.029/0.030/0.001 ms -->

# =========================================
# 6. 启动 Server/Injector（在 ns_server）
# =========================================
<!-- sudo ip netns exec ns_server ./server_app 8000 8000 -->
sudo ip netns exec ns_server ./server_app 8000 192.168.100.1 8000
#  参数说明：8000 = Server HTTP 监听端口
#            9000 = Client HTTP 服务端口（test_client - 第二参数）

# =========================================
# 7. 启动 Client/Reader（在 ns_client）
# =========================================
sudo ip netns exec ns_client ./client_app 192.168.100.2 8000

#  参数说明：192.168.100.2 = 管理链路 Server IP
#            9000           = Client HTTP 服务端口

# （可选）在另一个终端监控隧道流量：
# sudo ip netns exec ns_client tcpdump -n -i tun0
# sudo ip netns exec ns_server tcpdump -n -i tun0

# =========================================
# 8. 端到端验证（隧道流量）
# =========================================
sudo ip netns exec ns_client ping -c2 10.0.200.1
# 10.0.200.1 = Server 端 tun0 的地址

# 如果 ping 成功，说明：
#   10.0.100.1 → (tun0) → HTTP(send/poll) → (server tun0) → 10.0.200.1
# 双向隧道已打通，项目需求完成！

# =========================================
# 9. 结果显示：
# =========================================
# 启动 Server/Injector（在 ns_server）：
xinyi-xu@xinyi-xu-VirtualBox:~/GitHub/ip_over_discord$ sudo ip netns exec ns_server ./server_app 8000 9000
[S] main begins: port=8000, client_port=9000
[TUN] allocated tun0 (fd=3)
[S] tun0 fd=3
[HTTP-SRV] listening on port 8000
[S] HTTP server up on 8000
[H-CLT] init_send → http://127.0.0.1:9000/send
[S] send-handle ready
[H-CLT] init_poll → http://127.0.0.1:8000/poll
[S] poll-handle ready
[S] reader_to_client thread spawned
[INJ] start injector on tun fd=3
[S] injector thread spawned
[INJ] injector_loop running
[server] queued 84 bytes from /send
[server] queued 0 bytes from /send
[server] /poll returning 84 bytes
[server] /poll returning 0 bytes
[server] queued 84 bytes from /send
[server] /poll returning 84 bytes


# 启动 Client/Reader（在 ns_client）
xinyi-xu@xinyi-xu-VirtualBox:~/GitHub/ip_over_discord$ sudo ip netns exec ns_client ./client_app 192.168.100.2 8000
[C] main begins: server_url=http://192.168.100.2:8000
[TUN] allocated tun0 (fd=3)
[C] tun_alloc -> tun0 fd=3
[HTTP-SRV] listening on port 8000
[C] HTTP server up on 8000
[H-CLT] init_send → http://192.168.100.2:8000/send
[C] send-handle ready
[H-CLT] init_poll → http://192.168.100.2:8000/poll
[C] poll-handle ready
[C] injector_from_server thread spawned
[C] read 84 bytes from tun, sending
[C] got 84 bytes from poll, writing to tun
[C] read 84 bytes from tun, sending
[C] got 84 bytes from poll, writing to tun

# 在另一个终端监控隧道流量：
# 终端A：
xinyi-xu@xinyi-xu-VirtualBox:~/GitHub/ip_over_discord$ sudo ip netns exec ns_client tcpdump -n -i tun0 icmp
tcpdump: verbose output suppressed, use -v[v]... for full protocol decode
listening on tun0, link-type RAW (Raw IP), snapshot length 262144 bytes
^C22:51:51.089358 IP 10.0.100.1 > 10.0.200.1: ICMP echo request, id 4650, seq 1, length 64
22:51:51.098870 IP 10.0.100.1 > 10.0.200.1: ICMP echo request, id 4650, seq 1, length 64
22:51:52.119965 IP 10.0.100.1 > 10.0.200.1: ICMP echo request, id 4650, seq 2, length 64
22:51:52.127591 IP 10.0.100.1 > 10.0.200.1: ICMP echo request, id 4650, seq 2, length 64

4 packets captured
4 packets received by filter
0 packets dropped by kernel

# 终端B：
xinyi-xu@xinyi-xu-VirtualBox:~/GitHub/ip_over_discord$ sudo ip netns exec ns_server tcpdump -n -i tun0 icmp
tcpdump: verbose output suppressed, use -v[v]... for full protocol decode
listening on tun0, link-type RAW (Raw IP), snapshot length 262144 bytes
^C
0 packets captured
0 packets received by filter
0 packets dropped by kernel


# 端到端验证（隧道流量）
p_over_discord$ sudo ip netns exec ns_client ping -c2 10.0.200.1
PING 10.0.200.1 (10.0.200.1) 56(84) bytes of data.

--- 10.0.200.1 ping statistics ---
2 packets transmitted, 0 received, 100% packet loss, time 1031ms




# 直接用 curl 验证 HTTP 管理平面
xinyi-xu@xinyi-xu-VirtualBox:~/GitHub/ip_over_discord$ ip netns exec ns_server curl -v -X GET http://127.0.0.1:8000/poll
setting the network namespace "ns_server" failed: Operation not permitted

xinyi-xu@xinyi-xu-VirtualBox:~/GitHub/ip_over_discord$ ip netns exec ns_client curl -v -X POST --data-binary '' http://127.0.0.1:9000/send
setting the network namespace "ns_client" failed: Operation not permitted

# 确保进程真的在各自 namespace 内
xinyi-xu@xinyi-xu-VirtualBox:~/GitHub/ip_over_discord$ ps aux | grep server_app
root      189187  0.0  0.1  28552  7476 pts/0    S+   22:29   0:00 sudo ip netns exec ns_server ./server_app 8000 9000
root      189188  0.0  0.0  28552  2692 pts/2    Ss   22:29   0:00 sudo ip netns exec ns_server ./server_app 8000 9000
root      189189  7.5  0.2 323640  9792 pts/2    Sl+  22:29   0:12 ./server_app 8000 9000
xinyi-xu  203012  0.0  0.0  17836  2236 pts/5    S+   22:32   0:00 grep --color=auto server_app

xinyi-xu@xinyi-xu-VirtualBox:~/GitHub/ip_over_discord$ ps aux | grep client_app
root      190906  0.0  0.1  28376  7188 pts/1    S+   22:30   0:00 sudo ip netns exec ns_client ./client_app 192.168.100.2 9000
root      190907  0.0  0.0  28376  2604 pts/3    Ss   22:30   0:00 sudo ip netns exec ns_client ./client_app 192.168.100.2 9000
root      190908  2.5  0.2 168044  9936 pts/3    Sl+  22:30   0:04 ./client_app 192.168.100.2 9000
xinyi-xu  204685  0.0  0.0  17836  2296 pts/5    S+   22:33   0:00 grep --color=auto client_app