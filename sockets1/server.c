#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>

#define BUF_SIZE 1024
#define NAME_SIZE 64

static int server_fd = -1;

static void handle_sigint(int sig) {
    (void)sig;
    printf("\nClosing the server application...\n");
    if (server_fd != -1)
        close(server_fd);
    exit(0);
}

void chat(int client_fd) {
    char my_name[NAME_SIZE];
    char their_name[NAME_SIZE];
    char buf[BUF_SIZE];
    ssize_t n;

    printf("Enter your username: ");
    fflush(stdout);
    if (!fgets(my_name, sizeof(my_name), stdin))
        return;
    my_name[strcspn(my_name, "\n")] = '\0';

    send(client_fd, my_name, strlen(my_name), 0);

    n = recv(client_fd, their_name, sizeof(their_name) - 1, 0);
    if (n <= 0)
        return;
    their_name[n] = '\0';

    printf("%s joined the chat. They go first.\n", their_name);

    while (1) {
        n = recv(client_fd, buf, sizeof(buf) - 1, 0);
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

        printf("%s: ", my_name);
        fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin))
            break;
        buf[strcspn(buf, "\n")] = '\0';

        send(client_fd, buf, strlen(buf), 0);

        if (strcmp(buf, "/quit") == 0)
            break;
    }

    close(client_fd);
    printf("Chat ended. Waiting for a new connection...\n\n");
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port: %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, handle_sigint);

    int client_fd;
    struct sockaddr_in address;
    socklen_t addr_len = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 1) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n\n", port);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&address, &addr_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }
        printf("Client connected from %s\n", inet_ntoa(address.sin_addr));
        chat(client_fd);
    }

    close(server_fd);
    return 0;
}
