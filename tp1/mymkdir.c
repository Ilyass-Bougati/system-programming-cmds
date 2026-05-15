#include <stdio.h>
#include <sys/stat.h>

int main(int argc, char **argv) {
    if (argc < 2)
    {
        printf("Error: Usage is as follows\n");
        printf("\t%s [Directory1] [Directory2] ...\n", argv[0]);
    }

    for (int i = 1; i < argc; i++)
    {
        char *dir_path = argv[i];
        if (mkdir(dir_path, 0777) == 0)
        {
            printf("%s\t\e[0;32mwas created successfully\e[0m\n", dir_path);
        } else {
            fprintf(stderr, "%s\t\e[0;31mwasn't created\e[0m\n", dir_path);
        }
    }
}