CC = gcc
CFLAGS = -Wall -O2 `pkg-config --cflags libcurl`
LDFLAGS = `pkg-config --libs libcurl` -lpthread

SRCS = tun.c discord.c base64.c
OBJS = $(SRCS:.c=.o)

all: client service

client: $(OBJS) client.o
	$(CC) -o $@ $^ $(LDFLAGS)

service: $(OBJS) service.o
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f *.o client service
