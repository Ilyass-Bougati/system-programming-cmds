#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>

#include "common.h"

typedef struct {
    int    fd;
    char   name[NAME_SIZE];
    time_t joined_at;
    int    in_use;
} client_t;

static client_t        clients[MAX_CLIENTS];
static pthread_mutex_t clients_mu = PTHREAD_MUTEX_INITIALIZER;
static int             server_fd  = -1;
static FILE           *log_fp     = NULL;
static pthread_mutex_t log_mu     = PTHREAD_MUTEX_INITIALIZER;

static void log_message(const char *fmt, ...) {
    if (!log_fp) return;
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);
    pthread_mutex_lock(&log_mu);
    fprintf(log_fp, "%s ", ts);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(log_fp, fmt, ap);
    va_end(ap);
    fputc('\n', log_fp);
    fflush(log_fp);
    pthread_mutex_unlock(&log_mu);
}

static int valid_name(const char *s) {
    if (!*s) return 0;
    if (strlen(s) >= NAME_SIZE) return 0;
    for (const char *p = s; *p; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '-'))
            return 0;
    }
    return 1;
}

/* register_client: returns slot index, -1 if name taken, -2 if server full. */
static int register_client(int fd, const char *name) {
    pthread_mutex_lock(&clients_mu);
    int slot = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].in_use && strcmp(clients[i].name, name) == 0) {
            pthread_mutex_unlock(&clients_mu);
            return -1;
        }
        if (slot < 0 && !clients[i].in_use)
            slot = i;
    }
    if (slot < 0) {
        pthread_mutex_unlock(&clients_mu);
        return -2;
    }
    clients[slot].fd        = fd;
    clients[slot].joined_at = time(NULL);
    clients[slot].in_use    = 1;
    strncpy(clients[slot].name, name, NAME_SIZE - 1);
    clients[slot].name[NAME_SIZE - 1] = '\0';
    pthread_mutex_unlock(&clients_mu);
    return slot;
}

static void unregister_client(int slot) {
    pthread_mutex_lock(&clients_mu);
    clients[slot].in_use  = 0;
    clients[slot].fd      = -1;
    clients[slot].name[0] = '\0';
    pthread_mutex_unlock(&clients_mu);
}

/* Snapshot fds under lock then send outside the critical section so a slow
   client cannot stall others. SO_SNDTIMEO bounds each send. */
static void broadcast(const char *line, int except_fd) {
    int fds[MAX_CLIENTS];
    int n = 0;
    pthread_mutex_lock(&clients_mu);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].in_use && clients[i].fd != except_fd)
            fds[n++] = clients[i].fd;
    }
    pthread_mutex_unlock(&clients_mu);
    for (int i = 0; i < n; i++)
        (void)send_line(fds[i], line);
}

static int send_private(const char *target, const char *line) {
    int fd = -1;
    pthread_mutex_lock(&clients_mu);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].in_use && strcmp(clients[i].name, target) == 0) {
            fd = clients[i].fd;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mu);
    if (fd < 0) return -1;
    return send_line(fd, line);
}

static void handle_list(int fd) {
    char reply[BUF_SIZE];
    size_t off = 0;
    off += (size_t)snprintf(reply + off, sizeof(reply) - off, "LIST");
    pthread_mutex_lock(&clients_mu);
    for (int i = 0; i < MAX_CLIENTS && off < sizeof(reply) - 1; i++) {
        if (clients[i].in_use) {
            int w = snprintf(reply + off, sizeof(reply) - off,
                             " %s", clients[i].name);
            if (w < 0) break;
            off += (size_t)w;
        }
    }
    pthread_mutex_unlock(&clients_mu);
    send_line(fd, reply);
}

static void *client_thread(void *arg) {
    int fd = (int)(intptr_t)arg;
    line_reader_t lr;
    lr_init(&lr, fd);

    char line[BUF_SIZE];
    char name[NAME_SIZE] = {0};
    int  slot = -1;

    /* Handshake: keep asking until we get a valid, non-taken NICK. */
    while (slot < 0) {
        int r = lr_next(&lr, line, sizeof(line));
        if (r <= 0) goto done;

        if (strncmp(line, "NICK ", 5) != 0) {
            send_line(fd, "ERR expected NICK first");
            continue;
        }
        const char *requested = line + 5;
        if (!valid_name(requested)) {
            send_line(fd, "ERR invalid name (alnum, _-, max 31 chars)");
            continue;
        }
        int s = register_client(fd, requested);
        if (s == -1) { send_line(fd, "ERR name already taken"); continue; }
        if (s == -2) { send_line(fd, "ERR server full");        goto done; }
        slot = s;
        strncpy(name, requested, NAME_SIZE - 1);
        name[NAME_SIZE - 1] = '\0';
        send_line(fd, "OK");
    }

    char ann[BUF_SIZE];
    snprintf(ann, sizeof(ann), "JOIN %s", name);
    broadcast(ann, fd);
    printf("[+] %s joined (fd=%d)\n", name, fd);
    log_message("[JOIN] %s", name);

    for (;;) {
        int r = lr_next(&lr, line, sizeof(line));
        if (r <= 0) break;

        if (strcmp(line, "QUIT") == 0) {
            break;
        }
        if (strcmp(line, "LIST") == 0) {
            handle_list(fd);
            continue;
        }
        if (strncmp(line, "MSG ", 4) == 0) {
            char out[BUF_SIZE];
            snprintf(out, sizeof(out), "MSG %s %s", name, line + 4);
            broadcast(out, fd);
            log_message("[MSG] %s: %s", name, line + 4);
            continue;
        }
        if (strncmp(line, "PRIV ", 5) == 0) {
            const char *rest = line + 5;
            const char *sp   = strchr(rest, ' ');
            if (!sp || sp == rest) {
                send_line(fd, "ERR usage: PRIV <name> <text>");
                continue;
            }
            size_t tlen = (size_t)(sp - rest);
            if (tlen >= NAME_SIZE) {
                send_line(fd, "ERR target name too long");
                continue;
            }
            char target[NAME_SIZE];
            memcpy(target, rest, tlen);
            target[tlen] = '\0';
            const char *text = sp + 1;
            if (strcmp(target, name) == 0) {
                send_line(fd, "ERR cannot PRIV yourself");
                continue;
            }
            char out[BUF_SIZE];
            snprintf(out, sizeof(out), "PRIV %s %s", name, text);
            if (send_private(target, out) < 0) {
                char err[BUF_SIZE];
                snprintf(err, sizeof(err), "ERR no such user: %s", target);
                send_line(fd, err);
            } else {
                log_message("[PRIV] %s -> %s: %s", name, target, text);
            }
            continue;
        }
        send_line(fd, "ERR unknown command");
    }

done:
    if (slot >= 0) {
        unregister_client(slot);
        char leave[BUF_SIZE];
        snprintf(leave, sizeof(leave), "LEAVE %s", name);
        broadcast(leave, fd);
        printf("[-] %s left\n", name);
        log_message("[LEAVE] %s", name);
    }
    shutdown(fd, SHUT_RDWR);
    close(fd);
    return NULL;
}

static void handle_sigint(int sig) {
    (void)sig;
    printf("\nShutting down Chat Hub...\n");
    if (server_fd != -1) close(server_fd);
    if (log_fp) fclose(log_fp);
    _exit(0);
}

int main(int argc, char *argv[]) {
    int         port     = 5555;
    const char *log_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--log") == 0 && i + 1 < argc) {
            log_path = argv[++i];
        } else {
            fprintf(stderr, "Usage: %s [--port <port>] [--log <file>]\n", argv[0]);
            return EXIT_FAILURE;
        }
    }
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port: %d\n", port);
        return EXIT_FAILURE;
    }
    if (log_path) {
        log_fp = fopen(log_path, "a");
        if (!log_fp) { perror("fopen log"); return EXIT_FAILURE; }
    }

    signal(SIGINT, handle_sigint);
    signal(SIGPIPE, SIG_IGN);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return EXIT_FAILURE; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port        = htons(port),
    };

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); return EXIT_FAILURE;
    }
    if (listen(server_fd, 16) < 0) {
        perror("listen"); return EXIT_FAILURE;
    }

    printf("Chat Hub listening on port %d...\n", port);

    for (;;) {
        struct sockaddr_in cli;
        socklen_t cl = sizeof(cli);
        int fd = accept(server_fd, (struct sockaddr *)&cli, &cl);
        if (fd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            break;
        }
        printf("[~] connection from %s:%d\n",
               inet_ntoa(cli.sin_addr), ntohs(cli.sin_port));

        /* Bound per-send time so a stalled client can't block broadcasts. */
        struct timeval to = { .tv_sec = 5, .tv_usec = 0 };
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &to, sizeof(to));

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread,
                           (void *)(intptr_t)fd) != 0) {
            perror("pthread_create");
            close(fd);
            continue;
        }
        pthread_detach(tid);
    }

    if (log_fp) fclose(log_fp);
    close(server_fd);
    return 0;
}
