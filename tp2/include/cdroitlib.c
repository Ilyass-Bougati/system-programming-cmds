#include "cdroitlib.h"
#include "sys/stat.h"

__mode_t get_mode(char flag, char perm, __mode_t *mode)
{
    switch (flag)
    {
        case 'u':
            switch (perm)
            {
                case 'r':
                    *mode = *mode ^ S_IRUSR;
                    break;
                
                case 'w':
                    *mode = *mode ^ S_IWUSR;
                    break;
                    
                case 'x':
                    *mode = *mode ^ S_IXUSR;
                    break;
            }
            break;

        case 'o':
            switch (perm)
            {
                case 'r':
                    *mode = *mode ^ S_IROTH;
                    break;
                
                case 'w':
                    *mode = *mode ^ S_IWOTH;
                    break;
                    
                case 'x':
                    *mode = *mode ^ S_IXOTH;
                    break;
            }
            break;

        case 'g':
            switch (perm)
            {
                case 'r':
                    *mode = *mode ^ S_IRGRP;
                    break;
                
                case 'w':
                    *mode = *mode ^ S_IWGRP;
                    break;
                    
                case 'x':
                    *mode = *mode ^ S_IXGRP;
                    break;
            }
            break;
    }

    return *mode;
}