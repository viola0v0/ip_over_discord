# Makefile for IP\_OVER\_FOO project

CC = gcc
CFLAGS = -Iinclude -Wall -Wextra -O2
LDFLAGS = -lcurl -lmicrohttpd -lpthread

SRC\_DIR = src
BIN\_DIR = bin

CLIENT\_SRC = \$(SRC\_DIR)/client.c&#x20;
\$(SRC\_DIR)/http\_client.c&#x20;
\$(SRC\_DIR)/tun.c
SERVER\_SRC = \$(SRC\_DIR)/server.c&#x20;
\$(SRC\_DIR)/http\_server.c&#x20;
\$(SRC\_DIR)/injector.c&#x20;
\$(SRC\_DIR)/tun.c

CLIENT\_BIN = \$(BIN\_DIR)/client
SERVER\_BIN = \$(BIN\_DIR)/server

.PHONY: all clean dirs

all: dirs \$(CLIENT\_BIN) \$(SERVER\_BIN)

dirs:
@mkdir -p \$(BIN\_DIR)

\$(CLIENT\_BIN): \$(CLIENT\_SRC)
\$(CC) \$(CFLAGS) -o \$@ \$(CLIENT\_SRC) \$(LDFLAGS)

\$(SERVER\_BIN): \$(SERVER\_SRC)
\$(CC) \$(CFLAGS) -o \$@ \$(SERVER\_SRC) \$(LDFLAGS)

clean:
rm -rf \$(BIN\_DIR)