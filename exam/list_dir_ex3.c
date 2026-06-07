#include <stdio.h>      // printf, fprintf, fgets, perror
#include <stdlib.h>     // exit, EXIT_FAILURE
#include <string.h>     // strcspn
#include <dirent.h>     // opendir, readdir, closedir, DIR, struct dirent

// buffer size for the directory path entered by the user
#define PATH_SIZE 1024

int main(void) {
    char path[PATH_SIZE];   // holds the directory path typed by the user

    // prompting the user for a directory path
    printf("Enter a directory path: ");
    fflush(stdout);

    // fgets() reads a whole line (including the trailing newline) from stdin
    // into path. It returns NULL on end-of-file or error, in which case we stop.
    if (!fgets(path, sizeof(path), stdin)) {
        fprintf(stderr, "No input.\n");
        exit(EXIT_FAILURE);
    }
    // fgets keeps the '\n'; strcspn finds its offset so we can cut it off,
    // otherwise the newline would be part of the path and opendir would fail.
    path[strcspn(path, "\n")] = '\0';

    // dir : the directory stream. opendir() opens the directory for reading and
    //       returns a DIR* handle, or NULL if the path is invalid / not a
    //       directory / not accessible (errno is set, hence perror).
    DIR *dir = opendir(path);
    if (!dir) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    printf("Contents of %s:\n", path);

    // entry : points to one directory entry per readdir() call. readdir()
    //         returns the next entry each time, or NULL when there are no more.
    //         entry->d_name is the item's name (file or subdirectory).
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        printf("%s\n", entry->d_name);
    }

    // closedir() releases the directory stream opened by opendir().
    closedir(dir);
    return 0;
}
