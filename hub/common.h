#ifndef HUB_COMMON_H
#define HUB_COMMON_H

#include <stddef.h>
#include <string.h>
#include <sys/socket.h>

#define BUF_SIZE     1024
#define NAME_SIZE      32
#define MAX_CLIENTS    64

/* Wire protocol — line-based, '\n' terminated, first token is the verb.
 *
 * Client → Server:
 *   NICK <name>
 *   MSG  <text>
 *   PRIV <name> <text>
 *   LIST
 *   QUIT
 *
 * Server → Client:
 *   OK
 *   ERR  <reason>
 *   MSG  <from> <text>
 *   PRIV <from> <text>
 *   JOIN <name>
 *   LEAVE <name>
 *   LIST <name1> <name2> ...
 *   SYS  <text>
 */

/* Per-fd line reader. Calls recv() into an internal buffer and hands back
   complete '\n'-terminated lines (with the '\n' stripped).
   Returns 1 if a line was produced, 0 on peer close, -1 on error. */
typedef struct {
    int    fd;
    char   buf[BUF_SIZE * 2];
    size_t len;
} line_reader_t;

static inline void lr_init(line_reader_t *lr, int fd) {
    lr->fd = fd;
    lr->len = 0;
}

static inline int lr_next(line_reader_t *lr, char *out, size_t out_cap) {
    for (;;) {
        char *nl = memchr(lr->buf, '\n', lr->len);
        if (nl) {
            size_t line_len = (size_t)(nl - lr->buf);
            size_t copy = line_len < out_cap - 1 ? line_len : out_cap - 1;
            memcpy(out, lr->buf, copy);
            out[copy] = '\0';
            /* Strip a trailing '\r' for CRLF tolerance. */
            if (copy > 0 && out[copy - 1] == '\r')
                out[copy - 1] = '\0';
            size_t consumed = line_len + 1;
            memmove(lr->buf, lr->buf + consumed, lr->len - consumed);
            lr->len -= consumed;
            return 1;
        }
        if (lr->len >= sizeof(lr->buf)) {
            /* Overlong line — drop it to keep the connection alive. */
            lr->len = 0;
        }
        ssize_t n = recv(lr->fd, lr->buf + lr->len,
                         sizeof(lr->buf) - lr->len, 0);
        if (n == 0) return 0;
        if (n  < 0) return -1;
        lr->len += (size_t)n;
    }
}

/* send_all: write the full buffer or fail. Returns 0 on success, -1 on error. */
static inline int send_all(int fd, const char *data, size_t len) {
    while (len > 0) {
        ssize_t n = send(fd, data, len, MSG_NOSIGNAL);
        if (n <= 0) return -1;
        data += n;
        len  -= (size_t)n;
    }
    return 0;
}

static inline int send_line(int fd, const char *line) {
    size_t len = strlen(line);
    if (send_all(fd, line, len) < 0) return -1;
    return send_all(fd, "\n", 1);
}

#endif
