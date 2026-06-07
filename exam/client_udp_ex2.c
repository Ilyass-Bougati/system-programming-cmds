#include <stdio.h>      // printf, fprintf, perror
#include <stdlib.h>     // exit, atoi, EXIT_FAILURE
#include <string.h>     // strlen
#include <unistd.h>     // close
#include <arpa/inet.h>  // socket, sendto, recvfrom, htons, inet_pton,
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

    // sock_fd    : the file descriptor of our UDP socket, used both to send the
    //              datagram and to receive the reply.
    int sock_fd;
    // server_addr: the destination endpoint (server's IP/port) we send to, and
    //              which recvfrom() also fills in with the responder's address.
    struct sockaddr_in server_addr;
    // server_len : in/out length for recvfrom(); initialised to the struct size.
    socklen_t server_len = sizeof(server_addr);
    char buf[BUF_SIZE];     // holds the bytes received from the server
    ssize_t n;              // number of bytes actually received (or -1 on error)

    // socket(): create the endpoint.
    //   AF_INET    -> IPv4
    //   SOCK_DGRAM -> UDP (connectionless datagrams)
    //   0          -> default protocol (UDP)
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Describe where we want to send the datagram:
    server_addr.sin_family = AF_INET;          // IPv4
    server_addr.sin_port = htons(port);        // port in network byte order
    // inet_pton() = "presentation TO network": parses the dotted-decimal host
    // string ("127.0.0.1") into the 32-bit binary form stored in sin_addr.
    // Returns <= 0 if the string is not a valid IPv4 address.
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address: %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    // sendto() transmits one datagram to server_addr. UDP is connectionless,
    // so the destination address is supplied on every send rather than once via
    // connect(). strlen(msg) is the payload length (no NUL is sent on the wire).
    const char *msg = "Bonjour";
    if (sendto(sock_fd, msg, strlen(msg), 0,
               (struct sockaddr *)&server_addr, server_len) < 0) {
        perror("sendto");
        exit(EXIT_FAILURE);
    }
    printf("Sent: %s\n", msg);

    // recvfrom() blocks until the server's reply datagram arrives, copies its
    // payload into buf, and (re)fills server_addr with the sender's address.
    // It returns the number of bytes received in n.
    n = recvfrom(sock_fd, buf, sizeof(buf) - 1, 0,
                 (struct sockaddr *)&server_addr, &server_len);
    if (n < 0) {
        perror("recvfrom");
        exit(EXIT_FAILURE);
    }
    buf[n] = '\0';  // terminate the raw bytes so we can print them as a string
    printf("Received: %s\n", buf);

    // close() releases the socket descriptor.
    close(sock_fd);
    return 0;
}
