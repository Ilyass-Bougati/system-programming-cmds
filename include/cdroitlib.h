#ifndef CDROIT_H
#define CDROIT_H

#include "sys/types.h"
/*
    This function generates a mode based on the flags
*/
__mode_t get_mode(char flag, char perm, __mode_t *mode);


#endif