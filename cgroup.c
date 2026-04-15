#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <grp.h>
#include <sys/types.h>
#include "include/colors.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("This command change the group of a file\n");
        printf("Usage:\n");
        printf("\tcgroup %s[file_path] [group]%s\n", C_YELLOW, C_RESET);
        return 1;
    }

    const char *file_path = argv[1];
    const char *groupname = argv[2];

    struct group *grp = getgrnam(groupname);
    if (grp == NULL) {
        fprintf(stderr, "Error: Group '%s' does not exist.\n", groupname);
        return 1;
    }

    if (chown(file_path, -1, grp->gr_gid) == -1) {
        perror("Error changing group");
        return 1;
    }

    return 0;
}