#include <stdio.h>      // printf, fprintf, perror
#include <stdlib.h>     // exit, atoi, EXIT_FAILURE
#include <string.h>     // strlen
#include <unistd.h>     // close
#include <arpa/inet.h>  // socket, bind, listen, accept, recv, send, htons,
                        // inet_ntoa, struct sockaddr_in, INADDR_ANY

// buffer size for the exchanged messages
#define BUF_SIZE 1024

int main(int argc, char *argv[]) {
    // making sure the user provided a port
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // atoi() converts the textual port argument into an int; reject anything
    // outside the valid 1..65535 range.
    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port: %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    // server_fd : the LISTENING socket. It never carries user data; its only
    //             job is to accept incoming connections.
    // client_fd : the CONNECTED socket returned by accept(), one per client.
    //             All recv()/send() with that client happen on this descriptor.
    int server_fd, client_fd;
    // address   : doubles as (a) our local bind address before bind(), and
    //             (b) the storage accept() fills with the connecting client's
    //             address afterwards.
    struct sockaddr_in address;
    // addr_len  : in/out length for accept(); must be set to the struct size
    //             beforehand, and the kernel writes back the actual length.
    socklen_t addr_len = sizeof(address);
    char buf[BUF_SIZE];     // holds the bytes received from the client
    ssize_t n;              // number of bytes actually received (or -1 on error)

    // socket(): create the endpoint.
    //   AF_INET     -> IPv4
    //   SOCK_STREAM -> reliable, ordered byte stream = TCP (connection-oriented)
    //   0           -> default protocol (TCP)
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // SO_REUSEADDR lets us re-bind to the port immediately on restart instead
    // of waiting out the kernel's TIME_WAIT period on the old connection.
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Fill in the local address we want to listen on:
    address.sin_family = AF_INET;            // IPv4
    address.sin_addr.s_addr = INADDR_ANY;    // bind to every local interface
    address.sin_port = htons(port);          // port in network byte order

    // bind() ties the listening socket to that IP/port.
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // listen() switches the socket into passive mode so it can accept
    // connections. The second argument (backlog = 1) is the max number of
    // pending, not-yet-accepted connections the kernel will queue.
    if (listen(server_fd, 1) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("TCP server listening on port %d...\n", port);

    // accept() blocks until a client connects, then performs the TCP handshake
    // and returns a NEW descriptor (client_fd) dedicated to that connection.
    // It also writes the client's address into `address`/`addr_len`.
    client_fd = accept(server_fd, (struct sockaddr *)&address, &addr_len);
    if (client_fd < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }
    // inet_ntoa() converts the client's binary IP to a printable dotted string.
    printf("Client connected from %s\n", inet_ntoa(address.sin_addr));

    // recv() reads bytes from the connected stream socket. Because TCP is a
    // byte stream (not message-framed), recv() returns whatever has arrived so
    // far; n is the byte count (0 would mean the peer closed the connection).
    n = recv(client_fd, buf, sizeof(buf) - 1, 0);
    if (n < 0) {
        perror("recv");
        exit(EXIT_FAILURE);
    }
    buf[n] = '\0';  // terminate so we can print the received bytes as a string
    printf("Received: %s\n", buf);

    // send() writes our reply into the same connected stream. No destination
    // address is needed: TCP already knows the peer from the established
    // connection. strlen(reply) is the number of payload bytes to send.
    const char *reply = "Bonjour recu";
    if (send(client_fd, reply, strlen(reply), 0) < 0) {
        perror("send");
        exit(EXIT_FAILURE);
    }
    printf("Sent: %s\n", reply);

    // Close the per-client connection first, then the listening socket.
    close(client_fd);
    close(server_fd);
    return 0;
}
