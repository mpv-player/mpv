#include <stdlib.h>
#include <string.h>
#include "libavcodec/opt.h"

int parse_avopts(void *v, char *str){
    char *start;
    start= str= strdup(str);

    while(str && *str){
        char *next_opt, *arg;

        next_opt= strchr(str, ',');
        if(next_opt) *next_opt++= 0;

        arg     = strchr(str, '=');
        if(arg)      *arg++= 0;

        if(!av_set_string(v, str, arg)){
            free(start);
            return -1;
        }
        str= next_opt;
    }

    free(start);
    return 0;
}