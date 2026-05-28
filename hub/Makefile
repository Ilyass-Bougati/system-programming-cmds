CC = gcc
CFLAGS = -Wall -Wextra -pthread

all: chat_serverd chat_client

chat_serverd: server.c common.h
	$(CC) $(CFLAGS) -o $@ server.c

chat_client: client.c common.h
	$(CC) $(CFLAGS) -o $@ client.c

clean:
	rm -f chat_serverd chat_client

.PHONY: all clean
