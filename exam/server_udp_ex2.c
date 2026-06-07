#include <stdio.h>      // printf, fprintf, perror
#include <stdlib.h>     // exit, atoi, EXIT_FAILURE
#include <string.h>     // strlen
#include <unistd.h>     // close
#include <arpa/inet.h>  // socket, bind, recvfrom, sendto, htons, inet_ntoa,
                        // struct sockaddr_in, INADDR_ANY

// buffer size for the exchanged messages
#define BUF_SIZE 1024

int main(int argc, char *argv[]) {
    // making sure the user provided a port
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // atoi() converts the textual port argument ("5050") into an int.
    // We reject anything outside the valid TCP/UDP port range 1..65535.
    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port: %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    // server_fd  : the file descriptor of the UDP socket. Unlike TCP there is
    //              no separate "connection" descriptor; this single socket is
    //              used both to receive datagrams and to reply to them.
    int server_fd;
    // address    : the local endpoint we bind to (which IP/port WE listen on).
    // client_addr: filled in by recvfrom() with the SENDER's IP/port, so we
    //              know where to send the reply back to.
    struct sockaddr_in address, client_addr;
    // client_len : in/out length of client_addr. We must initialise it to the
    //              size of the buffer before recvfrom(); the kernel then writes
    //              back the actual size of the address it stored.
    socklen_t client_len = sizeof(client_addr);
    char buf[BUF_SIZE];     // holds the bytes received from the client
    ssize_t n;              // number of bytes actually received (or -1 on error)

    // socket() creates the communication endpoint and returns its descriptor.
    //   AF_INET     -> IPv4 address family
    //   SOCK_DGRAM  -> datagram service = UDP (connectionless, message-oriented)
    //   0           -> default protocol for this type (UDP)
    server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // SO_REUSEADDR lets us re-bind to the same port immediately after the
    // program restarts, instead of waiting for the OS to release it.
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Filling in our local address structure:
    address.sin_family = AF_INET;            // IPv4
    address.sin_addr.s_addr = INADDR_ANY;    // accept datagrams on ANY local
                                             // interface (0.0.0.0)
    address.sin_port = htons(port);          // htons() = Host TO Network Short:
                                             // converts the port to the
                                             // network byte order (big-endian)
                                             // that the protocol requires.

    // bind() attaches the socket to our chosen IP/port so datagrams addressed
    // there are delivered to us. UDP has no listen()/accept(): once bound, the
    // socket can already receive. The cast to (struct sockaddr *) is the
    // generic-pointer convention the sockets API uses for all address families.
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    printf("UDP server listening on port %d...\n", port);

    // recvfrom() blocks until a datagram arrives, then:
    //   - copies up to BUF_SIZE-1 bytes of payload into buf
    //   - fills client_addr / client_len with the sender's address
    //   - returns the number of bytes received (n)
    // The "-1" leaves room for the NUL terminator we add below.
    n = recvfrom(server_fd, buf, sizeof(buf) - 1, 0,
                 (struct sockaddr *)&client_addr, &client_len);
    if (n < 0) {
        perror("recvfrom");
        exit(EXIT_FAILURE);
    }
    buf[n] = '\0';  // UDP payloads are raw bytes, not C strings: terminate it
                    // ourselves before printing with %s.
    // inet_ntoa() turns the sender's 32-bit binary IP into a dotted-decimal
    // string ("127.0.0.1") for display.
    printf("Received from %s: %s\n", inet_ntoa(client_addr.sin_addr), buf);

    // sendto() sends a datagram to an explicit destination. Because UDP keeps
    // no connection state, we must pass the client's address (captured by
    // recvfrom above) every time we reply.
    const char *reply = "Bonjour recu";
    if (sendto(server_fd, reply, strlen(reply), 0,
               (struct sockaddr *)&client_addr, client_len) < 0) {
        perror("sendto");
        exit(EXIT_FAILURE);
    }
    printf("Sent: %s\n", reply);

    // close() releases the socket descriptor and frees the bound port.
    close(server_fd);
    return 0;
}
