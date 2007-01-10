#include "config.h"

#include <sys/time.h>
#include <sys/timeb.h>
void gettimeofday(struct timeval* t,void* timezone)
{       struct timeb timebuffer;
        ftime( &timebuffer );
        t->tv_sec=timebuffer.time;
        t->tv_usec=1000*timebuffer.millitm;
}
