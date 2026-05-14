#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#define COPIES 100

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <prog1> [prog2 ...]\n", argv[0]);
        return 1;
    }

    int devnull = open("/dev/null", O_WRONLY);

    for (int i = 1; i < argc; i++) {
        char *exec_argv[] = { argv[i], NULL };
        int launched = 0;

        for (int j = 0; j < COPIES; j++) {
            pid_t pid = fork();
            if (pid < 0) {
                perror("fork");
                continue;
            }
            if (pid == 0) {
                if (devnull >= 0) {
                    dup2(devnull, STDOUT_FILENO);
                    dup2(devnull, STDERR_FILENO);
                }
                execvp(argv[i], exec_argv);
                _exit(127);
            }
            launched++;
        }
        printf("[spam] %d copies of '%s' launched\n", launched, argv[i]);
    }

    if (devnull >= 0)
        close(devnull);

    /* wait for all children so we leave no zombies */
    while (waitpid(-1, NULL, 0) > 0);

    return 0;
}
