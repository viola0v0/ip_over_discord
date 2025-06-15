CC        := gcc
CFLAGS    := -Iinclude -Wall -Wextra -O2
LIBS      := -lcurl -lpthread -lmicrohttpd

SRCS_SRV  := src/tun.c src/http_server.c src/injector.c src/http_client.c src/server.c
SRCS_CLI  := src/tun.c src/http_client.c src/http_server.c src/client.c

all: server_app client_app

server_app: $(SRCS_SRV)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

client_app: $(SRCS_CLI)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -f server_app client_app

.PHONY: all clean