#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pwd.h>
#include <sys/stat.h>

#define C_RESET  "\e[0m"
#define C_CYAN   "\e[4;36m"
#define C_YELLOW "\e[1;33m"

static char *file_type(mode_t mode) {
    if (S_ISREG(mode)) return "regular";
    if (S_ISDIR(mode)) return "directory";
    if (S_ISLNK(mode)) return "link";
    return "unknown";
}

static char *file_owner(uid_t uid) {
    return getpwuid(uid)->pw_name;
}

static char *human_file_size(off_t size) {
    static char buf[32];
    double s = size;
    const char *unit = "B";
    if (s >= 1024) { s /= 1024; unit = "KB"; }
    if (s >= 1024) { s /= 1024; unit = "MB"; }
    if (s >= 1024) { s /= 1024; unit = "GB"; }
    snprintf(buf, sizeof(buf), "%.2f %s", s, unit);
    return buf;
}

static char *formatted_time(time_t t) {
    static char buf[100];
    strftime(buf, sizeof(buf), "%A, %B %d, %Y - %H:%M:%S", localtime(&t));
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("USAGE:\n    myattr [FILENAME]\n");
        return 1;
    }

    struct stat s;
    if (stat(argv[1], &s) == -1) {
        perror("Error opening file");
        return 1;
    }

    printf(C_CYAN "Inode:" C_RESET "\t%lu\t\t\t " C_CYAN "Type:" C_RESET "\t%s\n",
           (unsigned long)s.st_ino, file_type(s.st_mode));
    printf(C_CYAN "Size:" C_RESET "\t%s\t\t " C_CYAN "Links:" C_RESET " %lu\n",
           human_file_size(s.st_size), (unsigned long)s.st_nlink);
    printf(C_CYAN "Owner:" C_RESET "\t%s " C_YELLOW "(uid: %u)" C_RESET "\n",
           file_owner(s.st_uid), (unsigned)s.st_uid);
    printf(C_CYAN "Date:" C_RESET "\t" C_YELLOW "%s" C_RESET "\n",
           formatted_time(s.st_ctime));

    return 0;
}
