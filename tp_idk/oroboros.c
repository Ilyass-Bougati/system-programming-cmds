/*
 * oroboros — the snake that eats its own tail.
 *
 * Builds a linked list of applications, launches one process per node,
 * then walks the list and kills every process it created.
 *
 * Usage: ./oroboros top vim zsh nano
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

typedef struct AppNode {
    char           *name;
    pid_t           pid;     /* set after fork */
    struct AppNode *next;
} AppNode;

/* ── list operations ──────────────────────────── */

static AppNode *node_new(const char *name)
{
    AppNode *n = malloc(sizeof *n);
    if (!n) { perror("malloc"); exit(1); }
    n->name = strdup(name);
    n->pid  = -1;
    n->next = NULL;
    return n;
}

/* Append and return the new node. */
static AppNode *list_append(AppNode **head, const char *name)
{
    AppNode *n = node_new(name);
    if (!*head) {
        *head = n;
        return n;
    }
    AppNode *cur = *head;
    while (cur->next)
        cur = cur->next;
    cur->next = n;
    return n;
}

static void list_print(AppNode *head)
{
    printf("[ ");
    for (AppNode *n = head; n; n = n->next)
        printf("%s(pid=%d)%s", n->name, n->pid, n->next ? " -> " : "");
    printf(" ]\n");
}

static void list_free(AppNode *head)
{
    while (head) {
        AppNode *next = head->next;
        free(head->name);
        free(head);
        head = next;
    }
}

/* ── launch / kill ────────────────────────────── */

static void launch(AppNode *node)
{
    char *exec_argv[] = { node->name, NULL };

    pid_t pid = fork();
    if (pid < 0)  { perror("fork"); return; }
    if (pid == 0) {
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        execvp(node->name, exec_argv);
        _exit(127);
    }
    node->pid = pid;
    printf("[+] launched '%s'  pid=%d\n", node->name, node->pid);
}

/*
 * Walk the list and kill every process.
 * Uses SIGTERM first, then SIGKILL after a grace period.
 */
static void kill_all(AppNode *head)
{
    printf("\n[~] sending SIGTERM to all...\n");
    for (AppNode *n = head; n; n = n->next) {
        if (n->pid <= 0) continue;
        if (kill(n->pid, SIGTERM) == 0)
            printf("[-] SIGTERM -> '%s' (pid=%d)\n", n->name, n->pid);
        else
            perror(n->name);
    }

    sleep(1);

    printf("[~] sending SIGKILL to survivors...\n");
    for (AppNode *n = head; n; n = n->next) {
        if (n->pid <= 0) continue;
        kill(n->pid, SIGKILL);   /* no-op if already dead (ESRCH) */
    }

    while (waitpid(-1, NULL, WNOHANG) > 0);
    printf("[*] all children reaped\n");
}

/* ── main ─────────────────────────────────────── */

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <prog1> [prog2 ...]\n", argv[0]);
        return 1;
    }

    AppNode *list = NULL;

    /* build the linked list from argv */
    for (int i = 1; i < argc; i++)
        list_append(&list, argv[i]);

    /* launch one process per node */
    printf("[oroboros] launching...\n");
    for (AppNode *n = list; n; n = n->next)
        launch(n);

    printf("\n[oroboros] current list state:\n");
    list_print(list);

    printf("\nPress <Enter> to kill everything and free the list...\n");
    getchar();

    kill_all(list);

    printf("\n[oroboros] freeing list...\n");
    list_free(list);
    printf("[oroboros] done. the snake has eaten its tail.\n");

    return 0;
}
