#include <stdio.h>
#include <unistd.h>
#include "include/colors.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("This deletes a file\n");
        printf("Usage:\n");
        printf("\tsuprimer %s[file_path]%s\n", C_YELLOW, C_RESET);
        return 1;
    }

    const char *file_path = argv[1];

    if (unlink(file_path) == -1) {
        perror("Error deleting file");
        return 1;   
    }
    
    return 0;
}