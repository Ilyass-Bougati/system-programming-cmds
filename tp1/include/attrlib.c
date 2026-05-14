#include "attrlib.h"
#include "pwd.h"
#include "colors.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void print_stats(struct stat file_stats)
{
    // file serial number (inode)
    long file_inode = (long) file_stats.st_ino;
    char *type = file_type(file_stats.st_mode);
    print_c("Inode:", C_CYAN);
    printf("\t%lu\t\t\t ", file_inode);
    print_c("Type:", C_CYAN);
    printf("\t%s\n", type);

    // size of file
    char *file_size = human_file_size(file_stats.st_size);
    print_c("Size:", C_CYAN);
    printf("\t%s\t\t ", file_size);
    print_c("Links:", C_CYAN);
    printf(" %lu\n", (unsigned long int) file_stats.st_nlink);

    // owner
    unsigned int user_id = (unsigned int) file_stats.st_uid;
    char *username = file_owner(file_stats.st_uid);
    print_c("Owner:", C_CYAN);
    printf("\t%s %s(uid: %u)%s\n", username, C_YELLOW, user_id, C_RESET);
    
    // creation time
    char *created_date = formatted_time(file_stats.st_ctime);
    print_c("Date:", C_CYAN);
    printf("\t%s%s%s\n", C_YELLOW, created_date, C_RESET);
}

char *formatted_time(__time_t c_time)
{
    time(&c_time);
    struct tm *time_info;
    time_info = localtime(&c_time);
    char *f_time = (char *) malloc(MAX_DATE_LENGTH * sizeof(char));

    strftime(f_time, MAX_DATE_LENGTH, "%A, %B %d, %Y - %H:%M:%S", time_info);
    return f_time;
}

char *file_type(__mode_t mode)
{
    char *type;
    if (S_ISREG(mode))
    {
        type = "regular";
    } else if (S_ISDIR(mode))
    {
        type = "directory";
    } else if (S_ISLNK(mode))
    {
        type = "link";
    } else
    {
        type = "unknown";
    }

    return type;
}

char *file_owner(__uid_t uid)
{
    struct passwd *user = getpwuid(uid);
    return user->pw_name;
}

char *human_file_size(__off_t size)
{
    // converting to long will make things easier
    double long_size = (double) size;
    
    // determining the unit
    char *unit;
    if (long_size >= 1024)
    {
        long_size /= 1024.0;
        unit = "KB";
    }
    if (long_size >= 1024.0)
    {
        long_size /= 1024.0;
        unit = "MB";
    }
    if (long_size >= 1024.0)
    {
        long_size /= 1024.0;
        unit = "GB";
    }

    // creating a buffer
    int length = 0;
    {
        long int temp = long_size;
        do {
            temp /= 10;
            length++;
        } while (temp > 1);
    }
    char *buffer = (char *) malloc((length + 5) * sizeof(char));
    sprintf(buffer, "%.2lf %s", long_size, unit);

    return buffer;
}