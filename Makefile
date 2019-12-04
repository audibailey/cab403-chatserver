CC = gcc
CFLAGS = -std=gnu99 -Wall -Wextra -g
LDFLAGS = -pthread -lm -lrt

serverSRC = $(wildcard src/server/*.c)
serverOBJ = $(serverSRC:.c=.o)

all: server client

server: $(serverOBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

client: src/client/main.c
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(serverOBJ) server client

