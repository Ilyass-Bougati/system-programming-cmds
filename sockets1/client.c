#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUF_SIZE 1024
#define NAME_SIZE 64

void chatc(int sock_fd) {
    char my_name[NAME_SIZE];
    char their_name[NAME_SIZE];
    char buf[BUF_SIZE];
    ssize_t n;

    n = recv(sock_fd, their_name, sizeof(their_name) - 1, 0);
    if (n <= 0) {
        fprintf(stderr, "Server closed connection.\n");
        return;
    }
    their_name[n] = '\0';

    printf("Enter your username: ");
    fflush(stdout);
    if (!fgets(my_name, sizeof(my_name), stdin))
        return;
    my_name[strcspn(my_name, "\n")] = '\0';

    send(sock_fd, my_name, strlen(my_name), 0);

    printf("Connected to %s. You go first!\n\n", their_name);

    while (1) {
        printf("%s: ", my_name);
        fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin))
            break;
        buf[strcspn(buf, "\n")] = '\0';

        send(sock_fd, buf, strlen(buf), 0);

        if (strcmp(buf, "/quit") == 0)
            break;

        n = recv(sock_fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) {
            printf("%s disconnected.\n", their_name);
            break;
        }
        buf[n] = '\0';

        if (strcmp(buf, "/quit") == 0) {
            printf("%s left the chat.\n", their_name);
            break;
        }
        printf("%s: %s\n", their_name, buf);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[2]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port: %s\n", argv[2]);
        exit(EXIT_FAILURE);
    }

    int sock_fd;
    struct sockaddr_in server_addr;

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address: %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    chatc(sock_fd);

    close(sock_fd);
    return 0;
}
