#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>

#define BUF_SIZE 1024
#define NAME_SIZE 64

static int server_fd = -1;

typedef struct {
    int fd;
    char peer[NAME_SIZE];
    volatile int *running;
} thread_arg_t;

static void handle_sigint(int sig) {
    (void)sig;
    printf("\nClosing the server application...\n");
    if (server_fd != -1)
        close(server_fd);
    exit(0);
}

static void *recv_thread(void *arg) {
    thread_arg_t *a = arg;
    char buf[BUF_SIZE];
    ssize_t n;

    while (*a->running) {
        n = recv(a->fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) {
            if (*a->running)
                printf("\n%s disconnected. Press Enter to exit.\n", a->peer);
            *a->running = 0;
            return NULL;
        }
        buf[n] = '\0';
        if (strcmp(buf, "/quit") == 0) {
            printf("\n%s left the chat. Press Enter to exit.\n", a->peer);
            *a->running = 0;
            return NULL;
        }
        /* \r\033[K: move to line start and clear it, so incoming messages
           overwrite the prompt the user may be typing on */
        printf("\r\033[K%s: %s\n", a->peer, buf);
        fflush(stdout);
    }
    return NULL;
}

static void chat(int client_fd, const char *peer_ip) {
    char my_name[NAME_SIZE], their_name[NAME_SIZE], buf[BUF_SIZE];
    ssize_t n;
    volatile int running = 1;

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

    printf("Connected with %s (%s). You can both type freely!\n\n", their_name, peer_ip);

    thread_arg_t arg = { .fd = client_fd, .running = &running };
    strncpy(arg.peer, their_name, NAME_SIZE - 1);
    arg.peer[NAME_SIZE - 1] = '\0';

    pthread_t tid;
    pthread_create(&tid, NULL, recv_thread, &arg);

    while (running) {
        printf("%s: ", my_name);
        fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin) || !running)
            break;
        buf[strcspn(buf, "\n")] = '\0';
        if (!running)
            break;
        send(client_fd, buf, strlen(buf), 0);
        if (strcmp(buf, "/quit") == 0) {
            running = 0;
            break;
        }
    }

    running = 0;
    shutdown(client_fd, SHUT_RDWR);
    pthread_join(tid, NULL);
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

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port        = htons(port)
    };

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 1) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n\n", port);

    socklen_t addr_len = sizeof(address);
    while (1) {
        int client_fd = accept(server_fd, (struct sockaddr *)&address, &addr_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }
        printf("Client connected from %s\n", inet_ntoa(address.sin_addr));
        chat(client_fd, inet_ntoa(address.sin_addr));
    }

    close(server_fd);
    return 0;
}
