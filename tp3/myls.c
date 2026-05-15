#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <unistd.h>

static void print_permissions(mode_t mode)
{
    if      (S_ISDIR(mode))  putchar('d');
    else if (S_ISLNK(mode))  putchar('l');
    else if (S_ISCHR(mode))  putchar('c');
    else if (S_ISBLK(mode))  putchar('b');
    else if (S_ISFIFO(mode)) putchar('p');
    else if (S_ISSOCK(mode)) putchar('s');
    else                     putchar('-');

    putchar((mode & S_IRUSR) ? 'r' : '-');
    putchar((mode & S_IWUSR) ? 'w' : '-');
    putchar((mode & S_IXUSR) ? 'x' : '-');
    putchar((mode & S_IRGRP) ? 'r' : '-');
    putchar((mode & S_IWGRP) ? 'w' : '-');
    putchar((mode & S_IXGRP) ? 'x' : '-');
    putchar((mode & S_IROTH) ? 'r' : '-');
    putchar((mode & S_IWOTH) ? 'w' : '-');
    putchar((mode & S_IXOTH) ? 'x' : '-');
}

static void print_long_entry(const char *fullpath, const char *name)
{
    struct stat st;
    if (lstat(fullpath, &st) < 0) {
        perror(fullpath);
        return;
    }

    print_permissions(st.st_mode);

    printf(" %2ld", (long)st.st_nlink);

    struct passwd *pw = getpwuid(st.st_uid);
    if (pw) printf(" %-8s", pw->pw_name);
    else    printf(" %-8d", st.st_uid);

    struct group *gr = getgrgid(st.st_gid);
    if (gr) printf(" %-8s", gr->gr_name);
    else    printf(" %-8d", st.st_gid);

    printf(" %8ld", (long)st.st_size);

    char timebuf[32];
    struct tm *tm_info = localtime(&st.st_mtime);
    strftime(timebuf, sizeof(timebuf), "%b %d %H:%M", tm_info);
    printf(" %s %s\n", timebuf, name);
}

static void list_dir(const char *path, int flag_l, int flag_R)
{
    DIR *dir = opendir(path);
    if (!dir) {
        perror(path);
        return;
    }

    char **subdirs = NULL;
    int subdir_count = 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;

        char fullpath[4096];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

        if (flag_l)
            print_long_entry(fullpath, entry->d_name);
        else
            printf("%s\n", entry->d_name);

        if (flag_R) {
            struct stat st;
            if (lstat(fullpath, &st) == 0 && S_ISDIR(st.st_mode)) {
                subdirs = realloc(subdirs, (subdir_count + 1) * sizeof(char *));
                if (!subdirs) { perror("realloc"); exit(1); }
                subdirs[subdir_count++] = strdup(fullpath);
            }
        }
    }

    closedir(dir);

    for (int i = 0; i < subdir_count; i++) {
        printf("\n%s:\n", subdirs[i]);
        list_dir(subdirs[i], flag_l, flag_R);
        free(subdirs[i]);
    }
    free(subdirs);
}

int main(int argc, char *argv[])
{
    int flag_l = 0, flag_R = 0;
    const char *target = ".";

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            for (int j = 1; argv[i][j] != '\0'; j++) {
                if      (argv[i][j] == 'l') flag_l = 1;
                else if (argv[i][j] == 'R') flag_R = 1;
                else {
                    fprintf(stderr, "myls: invalid option -- '%c'\n", argv[i][j]);
                    fprintf(stderr, "Usage: myls [-l] [-R] [directory]\n");
                    return 1;
                }
            }
        } else {
            target = argv[i];
        }
    }

    if (flag_R)
        printf("%s:\n", target);

    list_dir(target, flag_l, flag_R);

    return 0;
}
