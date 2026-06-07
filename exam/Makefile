CC = gcc
CFLAGS = -Wall -Wextra

all: server_udp_ex2 client_udp_ex2 server_tcp_ex2 client_tcp_ex2 list_dir_ex3

server_udp_ex2: server_udp_ex2.c
	$(CC) $(CFLAGS) -o $@ $<

client_udp_ex2: client_udp_ex2.c
	$(CC) $(CFLAGS) -o $@ $<

server_tcp_ex2: server_tcp_ex2.c
	$(CC) $(CFLAGS) -o $@ $<

client_tcp_ex2: client_tcp_ex2.c
	$(CC) $(CFLAGS) -o $@ $<

list_dir_ex3: list_dir_ex3.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f server_udp_ex2 client_udp_ex2 server_tcp_ex2 client_tcp_ex2 list_dir_ex3

.PHONY: all clean
