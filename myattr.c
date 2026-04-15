#include <stdio.h>
#include <sys/stat.h>
#include "include/attrlib.h"

int main(int argc, char** argv)
{
    if (argc < 2) {
        printf("USAGE : \n");
        printf("    attr [FILENAME]\n");
        return 1;
    }

    char *file_name = argv[1];
    struct stat file_info;
    if (stat(file_name, &file_info) == -1)
    {
        perror("Error opening file");
        return 1;
    }

    print_stats(file_info);
}