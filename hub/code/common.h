#ifndef HUB_COMMON_H
#define HUB_COMMON_H

#include <stddef.h>
#include <string.h>
#include <sys/socket.h>

#define BUF_SIZE     1024
#define NAME_SIZE      32
#define MAX_CLIENTS    64

/* Protocole « fil » — orienté ligne : chaque message se termine par '\n' et
 * son premier mot est le verbe.
 *
 * Client → Serveur :
 *   NICK <nom>          choix du pseudo (poignée de main)
 *   MSG  <texte>        diffusion à tous les autres clients
 *   PRIV <nom> <texte>  message privé vers <nom>
 *   LIST                demande la liste des connectés
 *   QUIT                déconnexion explicite
 *
 * Serveur → Client :
 *   OK                  pseudo accepté
 *   ERR  <raison>       pseudo refusé ou commande invalide
 *   MSG  <de> <texte>   message public reçu
 *   PRIV <de> <texte>   message privé reçu
 *   JOIN <nom>          un client vient d'arriver
 *   LEAVE <nom>         un client vient de partir
 *   LIST <n1> <n2> ...  réponse à LIST
 *   SYS  <texte>        message système
 */

/* Lecteur de lignes attaché à un descripteur de socket.
 *
 * TCP est un flux d'octets sans frontières de message : un seul recv() peut
 * renvoyer une demi-ligne, une ligne et demie, ou plusieurs lignes collées.
 * Cette structure accumule les octets reçus dans un tampon interne et ne
 * restitue au code applicatif que des lignes complètes (le '\n' est retiré). */
typedef struct {
    int    fd;
    char   buf[BUF_SIZE * 2];
    size_t len;                /* nombre d'octets actuellement dans buf */
} line_reader_t;

static inline void lr_init(line_reader_t *lr, int fd) {
    lr->fd = fd;
    lr->len = 0;
}

/* Restitue la prochaine ligne complète dans out (sans le '\n' final).
   Renvoie 1 si une ligne est prête, 0 si le pair a fermé la connexion,
   -1 en cas d'erreur. Bloque dans recv() tant qu'aucune ligne n'est dispo. */
static inline int lr_next(line_reader_t *lr, char *out, size_t out_cap) {
    for (;;) {
        /* Une ligne complète est-elle déjà présente dans le tampon ? */
        char *nl = memchr(lr->buf, '\n', lr->len);
        if (nl) {
            size_t line_len = (size_t)(nl - lr->buf);
            size_t copy = line_len < out_cap - 1 ? line_len : out_cap - 1;
            memcpy(out, lr->buf, copy);
            out[copy] = '\0';
            /* Tolérance CRLF : on retire un '\r' final éventuel. */
            if (copy > 0 && out[copy - 1] == '\r')
                out[copy - 1] = '\0';
            /* On décale le reste des octets au début du tampon. */
            size_t consumed = line_len + 1;
            memmove(lr->buf, lr->buf + consumed, lr->len - consumed);
            lr->len -= consumed;
            return 1;
        }
        /* Tampon plein sans aucun '\n' : ligne anormalement longue. On la
           jette pour ne pas figer définitivement la connexion. */
        if (lr->len >= sizeof(lr->buf))
            lr->len = 0;
        /* Aucune ligne disponible : on lit davantage d'octets. */
        ssize_t n = recv(lr->fd, lr->buf + lr->len,
                         sizeof(lr->buf) - lr->len, 0);
        if (n == 0) return 0;
        if (n  < 0) return -1;
        lr->len += (size_t)n;
    }
}

/* Envoie l'intégralité du tampon en bouclant : un seul send() peut n'accepter
   qu'une partie des données. Renvoie 0 si tout est parti, -1 en cas d'erreur. */
static inline int send_all(int fd, const char *data, size_t len) {
    while (len > 0) {
        /* MSG_NOSIGNAL : pas de SIGPIPE si le pair a fermé, on reçoit -1. */
        ssize_t n = send(fd, data, len, MSG_NOSIGNAL);
        if (n <= 0) return -1;
        data += n;
        len  -= (size_t)n;
    }
    return 0;
}

/* Envoie une ligne suivie de son '\n' terminal. */
static inline int send_line(int fd, const char *line) {
    size_t len = strlen(line);
    if (send_all(fd, line, len) < 0) return -1;
    return send_all(fd, "\n", 1);
}

#endif
