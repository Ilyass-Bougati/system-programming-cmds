#include <stdio.h>
#include <unistd.h>

#define C_RESET  "\e[0m"
#define C_YELLOW "\e[1;33m"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Deletes a file\n");
        printf("Usage:\n\tsuprimer " C_YELLOW "[file_path]" C_RESET "\n");
        return 1;
    }

    if (unlink(argv[1]) == -1) {
        perror("Error deleting file");
        return 1;
    }

    return 0;
}
