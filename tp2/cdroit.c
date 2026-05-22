#include <stdio.h>
#include <sys/stat.h>

#define C_RESET  "\e[0m"
#define C_YELLOW "\e[1;33m"

static void toggle_perm(mode_t *mode, char flag, char perm) {
    static const struct { char flag, perm; mode_t bit; } table[] = {
        {'u','r',S_IRUSR}, {'u','w',S_IWUSR}, {'u','x',S_IXUSR},
        {'g','r',S_IRGRP}, {'g','w',S_IWGRP}, {'g','x',S_IXGRP},
        {'o','r',S_IROTH}, {'o','w',S_IWOTH}, {'o','x',S_IXOTH},
    };
    for (int i = 0; i < 9; i++)
        if (table[i].flag == flag && table[i].perm == perm)
            *mode ^= table[i].bit;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        printf("Toggles a file permission for a user/group/other\n\n");
        printf(C_YELLOW "USAGE:\n" C_RESET);
        printf("\tcdroit " C_YELLOW "[file_path] [U/G/O] [R/W/X]" C_RESET "\n");
        return 1;
    }

    struct stat fi;
    if (stat(argv[1], &fi) == -1) {
        perror("Error opening file");
        return 1;
    }

    mode_t mode = fi.st_mode;
    toggle_perm(&mode, argv[2][0] | 0x20, argv[3][0] | 0x20);
    chmod(argv[1], mode);
    return 0;
}
