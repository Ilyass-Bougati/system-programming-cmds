#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#define BUF_SIZE 1024
#define NAME_SIZE 64

typedef struct {
    int fd;
    char peer[NAME_SIZE];
    volatile int *running;
} thread_arg_t;

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

static void chat(int sock_fd) {
    char my_name[NAME_SIZE], their_name[NAME_SIZE], buf[BUF_SIZE];
    ssize_t n;
    volatile int running = 1;

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

    printf("Connected with %s. You can both type freely!\n\n", their_name);

    thread_arg_t arg = { .fd = sock_fd, .running = &running };
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
        send(sock_fd, buf, strlen(buf), 0);
        if (strcmp(buf, "/quit") == 0) {
            running = 0;
            break;
        }
    }

    running = 0;
    shutdown(sock_fd, SHUT_RDWR);
    pthread_join(tid, NULL);
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

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(port)
    };

    if (inet_pton(AF_INET, argv[1], &addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address: %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    chat(sock_fd);
    close(sock_fd);
    return 0;
}
