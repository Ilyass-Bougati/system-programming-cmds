#include "attrlib.h"
#include <stdio.h>
#include <stdlib.h>

void print_stats(struct stat file_stats)
{
    // file serial number (inode)
    long file_inode = (long) file_stats.st_ino;
    char *type = file_type(file_stats.st_mode);
    printf("Inode: %lu\t\ttype: %s\n", file_inode, type);
    // size of file
    char *file_size = human_file_size(file_stats.st_size);
    printf("size: %s\n", file_size);
    
    // device
    // link count
    // owner
    // group
    
    // creation time
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