#include <stdio.h>
#include <grp.h>
#include <unistd.h>

#define C_RESET  "\e[0m"
#define C_YELLOW "\e[1;33m"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Changes the group of a file\n");
        printf("Usage:\n\tcgroup " C_YELLOW "[file_path] [group]" C_RESET "\n");
        return 1;
    }

    struct group *grp = getgrnam(argv[2]);
    if (!grp) {
        fprintf(stderr, "Error: Group '%s' does not exist.\n", argv[2]);
        return 1;
    }

    if (chown(argv[1], -1, grp->gr_gid) == -1) {
        perror("Error changing group");
        return 1;
    }

    return 0;
}
