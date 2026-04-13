#include <stdio.h>
#include "colors.h"

void print_c(char *str, char *COLOR)
{
    printf("%s%s%s", COLOR, str, C_RESET);
}