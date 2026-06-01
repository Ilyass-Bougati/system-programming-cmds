/*
 * server.c — Chat Hub : serveur de discussion centralisé.
 *
 * Modèle de concurrence : un thread POSIX par client. Le thread principal se
 * contente d'accepter les connexions (accept) puis confie chaque client à un
 * thread détaché qui exécute client_thread().
 *
 * État partagé : la table clients[] recense les connectés ; elle est protégée
 * par clients_mu. Le journal optionnel (--log) est protégé par log_mu.
 */

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
    time_t joined_at;            /* horodatage de connexion (exigé par le sujet) */
    int    in_use;               /* 0 = slot libre, 1 = slot occupé */
} client_t;

static client_t        clients[MAX_CLIENTS];
static pthread_mutex_t clients_mu = PTHREAD_MUTEX_INITIALIZER;
static int             server_fd  = -1;
static FILE           *log_fp     = NULL;
static pthread_mutex_t log_mu     = PTHREAD_MUTEX_INITIALIZER;

/* Écrit une ligne horodatée dans le journal, si --log est activé. Protégé par
   log_mu car plusieurs threads clients peuvent journaliser simultanément. */
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

/* Un pseudo valide est non vide, de longueur < NAME_SIZE, et composé
   uniquement d'alphanumériques, de '_' ou de '-'. */
static int valid_name(const char *s) {
    if (!*s) return 0;
    if (strlen(s) >= NAME_SIZE) return 0;
    for (const char *p = s; *p; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '-'))
            return 0;
    }
    return 1;
}

/* Insère un client dans la table, sous verrou.
   Renvoie l'indice du slot attribué, -1 si le pseudo est déjà pris,
   -2 si la table est pleine.
   La vérification d'unicité et l'insertion ont lieu dans la même section
   critique : aucun doublon ne peut donc se glisser entre les deux. */
static int register_client(int fd, const char *name) {
    pthread_mutex_lock(&clients_mu);
    int slot = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].in_use && strcmp(clients[i].name, name) == 0) {
            pthread_mutex_unlock(&clients_mu);
            return -1;                       /* pseudo déjà utilisé */
        }
        if (slot < 0 && !clients[i].in_use)
            slot = i;                        /* premier slot libre repéré */
    }
    if (slot < 0) {
        pthread_mutex_unlock(&clients_mu);
        return -2;                           /* table pleine */
    }
    clients[slot].fd        = fd;
    clients[slot].joined_at = time(NULL);
    clients[slot].in_use    = 1;
    strncpy(clients[slot].name, name, NAME_SIZE - 1);
    clients[slot].name[NAME_SIZE - 1] = '\0';
    pthread_mutex_unlock(&clients_mu);
    return slot;
}

/* Libère le slot d'un client (lors de sa déconnexion). */
static void unregister_client(int slot) {
    pthread_mutex_lock(&clients_mu);
    clients[slot].in_use  = 0;
    clients[slot].fd      = -1;
    clients[slot].name[0] = '\0';
    pthread_mutex_unlock(&clients_mu);
}

/* Diffuse une ligne à tous les clients sauf except_fd (en général l'émetteur).
 *
 * On copie d'abord les descripteurs cibles sous verrou, PUIS on envoie hors
 * verrou : ainsi un client lent — dont le send() peut bloquer — ne gèle ni la
 * table ni les autres threads. Le SO_SNDTIMEO posé à l'accept borne le temps
 * de chaque envoi. */
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

/* Diffuse une notification de la forme "VERBE pseudo" (JOIN ou LEAVE). */
static void broadcast_event(const char *verb, const char *name, int except_fd) {
    char line[BUF_SIZE];
    snprintf(line, sizeof(line), "%s %s", verb, name);
    broadcast(line, except_fd);
}

/* Envoie une ligne au seul client nommé target.
   Renvoie 0 si trouvé et envoyé, -1 si aucun client ne porte ce nom. */
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

/* Répond à LIST : construit "LIST <n1> <n2> ..." avec les pseudos connectés. */
static void handle_list(int fd) {
    char reply[BUF_SIZE];
    size_t off = (size_t)snprintf(reply, sizeof(reply), "LIST");
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

/* Traite "MSG <texte>" : diffuse le message public et le journalise. */
static void handle_msg(int fd, const char *name, const char *text) {
    char out[BUF_SIZE];
    snprintf(out, sizeof(out), "MSG %s %s", name, text);
    broadcast(out, fd);
    log_message("[MSG] %s: %s", name, text);
}

/* Traite "PRIV <dest> <texte>" : route le message vers le seul destinataire,
   ou renvoie une erreur si la syntaxe est invalide / la cible inconnue.
   args pointe juste après "PRIV ". */
static void handle_priv(int fd, const char *name, const char *args) {
    const char *sp = strchr(args, ' ');
    if (!sp || sp == args) {
        send_line(fd, "ERR usage: PRIV <name> <text>");
        return;
    }
    size_t tlen = (size_t)(sp - args);
    if (tlen >= NAME_SIZE) {
        send_line(fd, "ERR target name too long");
        return;
    }
    char target[NAME_SIZE];
    memcpy(target, args, tlen);
    target[tlen] = '\0';
    const char *text = sp + 1;

    if (strcmp(target, name) == 0) {
        send_line(fd, "ERR cannot PRIV yourself");
        return;
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
}

/* Poignée de main : lit des commandes NICK jusqu'à ce que le client propose un
   pseudo valide et disponible. Renvoie l'indice du slot attribué dans la table
   clients[], ou -1 si la connexion doit être abandonnée (déconnexion, erreur,
   ou serveur plein). En cas de succès, recopie le pseudo retenu dans out_name. */
static int do_handshake(line_reader_t *lr, int fd, char *out_name) {
    char line[BUF_SIZE];
    for (;;) {
        if (lr_next(lr, line, sizeof(line)) <= 0)
            return -1;                       /* déconnexion avant tout pseudo */

        if (strncmp(line, "NICK ", 5) != 0) {
            send_line(fd, "ERR expected NICK first");
            continue;
        }
        const char *requested = line + 5;
        if (!valid_name(requested)) {
            send_line(fd, "ERR invalid name (alnum, _-, max 31 chars)");
            continue;
        }
        int slot = register_client(fd, requested);
        if (slot == -1) { send_line(fd, "ERR name already taken"); continue; }
        if (slot == -2) { send_line(fd, "ERR server full");        return -1; }

        strncpy(out_name, requested, NAME_SIZE - 1);
        out_name[NAME_SIZE - 1] = '\0';
        send_line(fd, "OK");
        return slot;                         /* pseudo accepté */
    }
}

/* Boucle de service : lit et exécute les commandes du client jusqu'à QUIT ou
   déconnexion (lr_next renvoie alors <= 0). */
static void serve_client(line_reader_t *lr, int fd, const char *name) {
    char line[BUF_SIZE];
    while (lr_next(lr, line, sizeof(line)) > 0) {
        if (strcmp(line, "QUIT") == 0)
            break;
        else if (strcmp(line, "LIST") == 0)
            handle_list(fd);
        else if (strncmp(line, "MSG ", 4) == 0)
            handle_msg(fd, name, line + 4);
        else if (strncmp(line, "PRIV ", 5) == 0)
            handle_priv(fd, name, line + 5);
        else
            send_line(fd, "ERR unknown command");
    }
}

/* Cycle de vie complet d'un client, exécuté dans son propre thread :
   poignée de main → annonce JOIN → service des commandes → annonce LEAVE.
   Si la poignée de main échoue (slot < 0), on saute directement au nettoyage,
   qui est unique : plus besoin de goto. */
static void *client_thread(void *arg) {
    int fd = (int)(intptr_t)arg;
    line_reader_t lr;
    lr_init(&lr, fd);

    char name[NAME_SIZE] = {0};
    int slot = do_handshake(&lr, fd, name);

    if (slot >= 0) {
        broadcast_event("JOIN", name, fd);
        printf("[+] %s joined (fd=%d)\n", name, fd);
        log_message("[JOIN] %s", name);

        serve_client(&lr, fd, name);

        unregister_client(slot);
        broadcast_event("LEAVE", name, fd);
        printf("[-] %s left\n", name);
        log_message("[LEAVE] %s", name);
    }

    shutdown(fd, SHUT_RDWR);
    close(fd);
    return NULL;
}

/* Arrêt propre sur Ctrl-C : ferme la socket d'écoute et le journal. */
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

    /* Analyse des arguments : --port <n> et --log <fichier> (optionnels). */
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
    signal(SIGPIPE, SIG_IGN);     /* un client disparu ne doit pas nous tuer */

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return EXIT_FAILURE; }

    /* SO_REUSEADDR : réutiliser le port aussitôt après un arrêt du serveur. */
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

    /* Boucle d'acceptation : un thread détaché par client. */
    for (;;) {
        struct sockaddr_in cli;
        socklen_t cl = sizeof(cli);
        int fd = accept(server_fd, (struct sockaddr *)&cli, &cl);
        if (fd < 0) {
            if (errno == EINTR) continue;    /* interrompu par un signal */
            perror("accept");
            break;
        }
        printf("[~] connection from %s:%d\n",
               inet_ntoa(cli.sin_addr), ntohs(cli.sin_port));

        /* Borne le temps de chaque send() : un client gelé ne doit pas figer
           les diffusions vers les autres. */
        struct timeval to = { .tv_sec = 5, .tv_usec = 0 };
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &to, sizeof(to));

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread,
                           (void *)(intptr_t)fd) != 0) {
            perror("pthread_create");
            close(fd);
            continue;
        }
        pthread_detach(tid);                 /* aucune jointure : ressources
                                                libérées à la fin du thread */
    }

    if (log_fp) fclose(log_fp);
    close(server_fd);
    return 0;
}
