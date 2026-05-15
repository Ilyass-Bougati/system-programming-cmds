#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc < 2)
    {
        printf("Error: Usage is as follows\n");
        printf("\t%s [Directory1] [Directory2] ...\n", argv[0]);
    }

    for (int i = 1; i < argc; i++)
    {
        char *dir_path = argv[i];
        if (rmdir(dir_path) == 0)
        {
            printf("%s\t\e[0;32mwas deleted successfully\e[0m\n", dir_path);
        } else {
            fprintf(stderr, "%s\t\e[0;31mwasn't deleted\e[0m\n", dir_path);
        }
    }
}