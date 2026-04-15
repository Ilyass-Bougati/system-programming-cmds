#include <stdio.h>
#include <sys/stat.h>
#include "include/attrlib.h"
#include "include/cdroitlib.h"
#include "include/colors.h"

int main(int argc, char **argv)
{
    if (argc != 4) {
        printf("This command edits the permission of a certain file for a certain user\n");
        printf("if the user already has that permission, we remove it. Otherwise we add it\n\n");
        printf("%sUSAGE:%s\n", C_YELLOW, C_RESET);
        printf("\tcdroit %s[file_path] [U/G/O] [permission: R/W/X]%s\n\n", C_YELLOW, C_RESET);
    }

    char *file_path = argv[1];

    char flag = (argv[2][0] >= 'A' && argv[2][0] <= 'Z') 
                ? (argv[2][0] + ('a' - 'A')) : argv[2][0];

    char perm = (argv[3][0] >= 'A' && argv[3][0] <= 'Z') 
                ? (argv[3][0] + ('a' - 'A')) : argv[3][0];

    // reading the original mode
    struct stat file_info;
    if (stat(file_path, &file_info) == -1)
    {
        perror("Error opening file");
        return 1;
    }

    __mode_t mode = file_info.st_mode;
    get_mode(flag, perm, &mode);

    chmod(file_path, mode);
}