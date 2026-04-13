// won't use pragma once because it's not standard
#ifndef ATTRLIB_H
#define ATTRLIB_H

#include <sys/stat.h>
#include <bits/types.h>

#define MAX_DATE_LENGTH 100

/*
    This function prints the stats
*/
void print_stats(struct stat file_stats);

/*
    This functions gives the type of the file from its mode
*/
char *file_type(__mode_t mode);

/*
    This function converts the __off_t file size to a human readable format
*/
char *human_file_size(__off_t size);

/*
    Get file owner from the st_uid
*/
char *file_owner(__uid_t uid);

/*
    Format a time
*/
char *formatted_time(__time_t c_time);

#endif