#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include "include/colors.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("This command change the owner of a file\n");
        printf("Usage:\n");
        printf("\tcowner %s[file_path] [username]%s", C_YELLOW, C_RESET);
        return 1;
    }

    const char *file_path = argv[1];
    const char *username = argv[2];

    struct passwd *pwd = getpwnam(username);
    if (pwd == NULL) {
        fprintf(stderr, "Error: User '%s' does not exist.\n", username);
        return 1;
    }

    if (chown(file_path, pwd->pw_uid, -1) == -1) {
        perror("Error changing owner");
        return 1;
    }

    return 0;
}