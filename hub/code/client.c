/*
 * client.c — Client en ligne de commande du Chat Hub.
 *
 * Deux flux d'activité concurrents :
 *   - le thread principal lit le clavier (stdin) et envoie les commandes ;
 *   - un thread de réception (recv_thread) affiche les messages du serveur.
 * Le drapeau partagé « running » coordonne leur arrêt.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "common.h"

static volatile int running = 1;

/* Réaffiche l'invite de saisie. */
static void redraw_prompt(void) {
    printf("> ");
    fflush(stdout);
}

/* Thread de réception : affiche en continu les lignes envoyées par le serveur.
 *
 * Pour ne pas abîmer la ligne en cours de saisie, on efface d'abord la ligne
 * courante du terminal (séquence \r\033[K), on affiche l'évènement, puis on
 * réaffiche l'invite. L'argument est le line_reader_t de la socket. */
static void *recv_thread(void *arg) {
    line_reader_t *lr = arg;
    char line[BUF_SIZE];

    while (running) {
        if (lr_next(lr, line, sizeof(line)) <= 0) {
            /* 0 ou -1 : le serveur a fermé la connexion. */
            if (running) {
                printf("\r\033[K[disconnected from server]\n");
                fflush(stdout);
                running = 0;
            }
            return NULL;
        }
        printf("\r\033[K");                  /* efface l'invite courante */
        if (strncmp(line, "MSG ", 4) == 0) {
            /* "MSG <de> <texte>" → "de: texte" */
            const char *rest = line + 4;
            const char *sp   = strchr(rest, ' ');
            if (sp)
                printf("%.*s: %s\n", (int)(sp - rest), rest, sp + 1);
        } else if (strncmp(line, "PRIV ", 5) == 0) {
            /* "PRIV <de> <texte>" → message privé */
            const char *rest = line + 5;
            const char *sp   = strchr(rest, ' ');
            if (sp)
                printf("[private from %.*s] %s\n",
                       (int)(sp - rest), rest, sp + 1);
        } else if (strncmp(line, "JOIN ", 5) == 0) {
            printf("[*] %s a rejoint le chat\n", line + 5);
        } else if (strncmp(line, "LEAVE ", 6) == 0) {
            printf("[*] %s a quitté le chat\n", line + 6);
        } else if (strcmp(line, "LIST") == 0 || strncmp(line, "LIST ", 5) == 0) {
            printf("[users]%s\n", line + 4);
        } else if (strncmp(line, "ERR ", 4) == 0) {
            printf("[error] %s\n", line + 4);
        } else if (strncmp(line, "SYS ", 4) == 0) {
            printf("[sys] %s\n", line + 4);
        } else if (strcmp(line, "OK") == 0) {
            /* Attendu uniquement pendant la poignée de main ; ignoré ensuite. */
        } else {
            printf("%s\n", line);
        }
        redraw_prompt();
    }
    return NULL;
}

/* Poignée de main : envoie NICK <pseudo> et attend OK/ERR, en redemandant un
   pseudo tant que le serveur le refuse. Renvoie 0 dès qu'un pseudo est accepté
   (recopié dans out_name), -1 si la connexion est perdue. */
static int handshake(int fd, line_reader_t *lr, char *out_name, size_t cap) {
    char nick[NAME_SIZE];
    char line[BUF_SIZE];
    char msg[BUF_SIZE];

    for (;;) {
        printf("Pseudo: ");
        fflush(stdout);
        if (!fgets(nick, sizeof(nick), stdin)) return -1;
        nick[strcspn(nick, "\n")] = '\0';    /* retire le saut de ligne */
        if (nick[0] == '\0') continue;       /* saisie vide : on redemande */

        snprintf(msg, sizeof(msg), "NICK %s", nick);
        if (send_line(fd, msg) < 0) {
            fprintf(stderr, "send failed\n");
            return -1;
        }
        if (lr_next(lr, line, sizeof(line)) <= 0) {
            fprintf(stderr, "Server closed connection.\n");
            return -1;
        }
        if (strcmp(line, "OK") == 0) {
            strncpy(out_name, nick, cap - 1);
            out_name[cap - 1] = '\0';
            return 0;
        }
        if (strncmp(line, "ERR ", 4) == 0) {
            printf("[error] %s\n", line + 4);
            continue;                        /* pseudo refusé : on réessaie */
        }
        printf("Unexpected reply: %s\n", line);
    }
}

/* Résout host:port et ouvre une connexion TCP. Renvoie le descripteur, ou -1. */
static int connect_to(const char *host, int port) {
    char portstr[16];
    snprintf(portstr, sizeof(portstr), "%d", port);

    struct addrinfo hints = {
        .ai_family   = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    int err = getaddrinfo(host, portstr, &hints, &res);
    if (err != 0) {
        fprintf(stderr, "Cannot resolve %s: %s\n", host, gai_strerror(err));
        return -1;
    }
    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        perror("socket");
        freeaddrinfo(res);
        return -1;
    }
    if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");
        close(fd);
        freeaddrinfo(res);
        return -1;
    }
    freeaddrinfo(res);
    return fd;
}

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s --server <host> --port <port>\n", prog);
}

int main(int argc, char *argv[]) {
    const char *host = NULL;
    int         port = 0;

    /* Analyse des arguments : --server <hôte> et --port <n> (obligatoires). */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--server") == 0 && i + 1 < argc) {
            host = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else {
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }
    if (!host || port <= 0 || port > 65535) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    signal(SIGPIPE, SIG_IGN);     /* serveur disparu : send() renvoie -1 */

    int fd = connect_to(host, port);
    if (fd < 0) return EXIT_FAILURE;

    line_reader_t lr;
    lr_init(&lr, fd);

    char name[NAME_SIZE];
    if (handshake(fd, &lr, name, sizeof(name)) != 0) {
        close(fd);
        return EXIT_FAILURE;
    }
    printf("[INFO] Connecté en tant que %s\n", name);
    printf("Commandes: /msg <pseudo> <texte> | /users | /quit\n");
    redraw_prompt();

    /* Lance le thread de réception en lui passant directement le lecteur. */
    pthread_t tid;
    if (pthread_create(&tid, NULL, recv_thread, &lr) != 0) {
        perror("pthread_create");
        close(fd);
        return EXIT_FAILURE;
    }

    /* On borne la saisie sous BUF_SIZE pour que le préfixe protocolaire
       ("MSG ", etc.) tienne toujours dans msg[]. */
    char line[BUF_SIZE - NAME_SIZE - 16];
    while (running && fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '\0') { redraw_prompt(); continue; }

        if (strcmp(line, "/quit") == 0) {
            send_line(fd, "QUIT");
            running = 0;
            break;
        }
        if (strcmp(line, "/users") == 0) {
            send_line(fd, "LIST");
            continue;
        }
        if (strncmp(line, "/msg ", 5) == 0) {
            /* "/msg <pseudo> <texte>" → "PRIV <pseudo> <texte>" */
            const char *rest = line + 5;
            const char *sp   = strchr(rest, ' ');
            if (!sp || sp == rest) {
                printf("Usage: /msg <pseudo> <texte>\n");
                redraw_prompt();
                continue;
            }
            size_t tlen = (size_t)(sp - rest);
            if (tlen >= NAME_SIZE) {
                printf("Pseudo trop long\n");
                redraw_prompt();
                continue;
            }
            char msg[BUF_SIZE];
            snprintf(msg, sizeof(msg), "PRIV %.*s %s",
                     (int)tlen, rest, sp + 1);
            send_line(fd, msg);
            redraw_prompt();
            continue;
        }
        if (line[0] == '/') {
            printf("Commande inconnue. /msg, /users, /quit\n");
            redraw_prompt();
            continue;
        }
        /* Texte ordinaire → message public diffusé à tous. */
        char msg[BUF_SIZE];
        snprintf(msg, sizeof(msg), "MSG %s", line);
        send_line(fd, msg);
        redraw_prompt();
    }

    /* Arrêt : on réveille le thread de réception en fermant la socket. */
    running = 0;
    shutdown(fd, SHUT_RDWR);
    pthread_join(tid, NULL);
    close(fd);
    return 0;
}
