#include <stdio.h>
#include <pwd.h>
#include <unistd.h>

#define C_RESET  "\e[0m"
#define C_YELLOW "\e[1;33m"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Changes the owner of a file\n");
        printf("Usage:\n\tcowner " C_YELLOW "[file_path] [username]" C_RESET "\n");
        return 1;
    }

    struct passwd *pwd = getpwnam(argv[2]);
    if (!pwd) {
        fprintf(stderr, "Error: User '%s' does not exist.\n", argv[2]);
        return 1;
    }

    if (chown(argv[1], pwd->pw_uid, -1) == -1) {
        perror("Error changing owner");
        return 1;
    }

    return 0;
}
