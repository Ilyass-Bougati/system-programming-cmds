#include <stdio.h>      // printf, fprintf, perror
#include <stdlib.h>     // exit, atoi, EXIT_FAILURE
#include <string.h>     // strlen
#include <unistd.h>     // close
#include <arpa/inet.h>  // socket, connect, send, recv, htons, inet_pton,
                        // struct sockaddr_in

// buffer size for the exchanged messages
#define BUF_SIZE 1024

int main(int argc, char *argv[]) {
    // making sure the user provided a host and a port
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // atoi() converts the textual port argument into an int; reject anything
    // outside the valid 1..65535 range.
    int port = atoi(argv[2]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port: %s\n", argv[2]);
        exit(EXIT_FAILURE);
    }

    // sock_fd     : the connected TCP socket. After connect() succeeds, every
    //               send()/recv() flows over this single descriptor.
    int sock_fd;
    // server_addr : the destination endpoint (server's IP/port) we connect to.
    struct sockaddr_in server_addr;
    char buf[BUF_SIZE];     // holds the bytes received from the server
    ssize_t n;              // number of bytes actually received (or -1 on error)

    // socket(): create the endpoint.
    //   AF_INET     -> IPv4
    //   SOCK_STREAM -> reliable, ordered byte stream = TCP
    //   0           -> default protocol (TCP)
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Describe the server we want to reach:
    server_addr.sin_family = AF_INET;          // IPv4
    server_addr.sin_port = htons(port);        // port in network byte order
    // inet_pton() parses the dotted-decimal host string into binary form inside
    // sin_addr; returns <= 0 if the argument is not a valid IPv4 address.
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address: %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    // connect() performs the TCP three-way handshake with the server. Once it
    // returns successfully, the socket is bound to that peer and we no longer
    // pass an address to send()/recv().
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    // send() writes our greeting into the stream. strlen(msg) bytes are queued
    // for transmission; TCP guarantees they arrive in order and intact.
    const char *msg = "Bonjour";
    if (send(sock_fd, msg, strlen(msg), 0) < 0) {
        perror("send");
        exit(EXIT_FAILURE);
    }
    printf("Sent: %s\n", msg);

    // recv() blocks until the server's reply bytes arrive, copies them into buf,
    // and returns the byte count in n (0 would mean the server closed first).
    n = recv(sock_fd, buf, sizeof(buf) - 1, 0);
    if (n < 0) {
        perror("recv");
        exit(EXIT_FAILURE);
    }
    buf[n] = '\0';  // terminate so we can print the received bytes as a string
    printf("Received: %s\n", buf);

    // close() shuts down our end of the connection and releases the descriptor.
    close(sock_fd);
    return 0;
}
