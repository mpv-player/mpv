
#include <signal.h>
#include <time.h>
#include <sys/time.h>

#include "timer.h"

static struct itimerval it;
static struct sigaction sa;

timerTSigHandler timerSigHandler;

void timerSetHandler( timerTSigHandler handler )
{ timerSigHandler=handler; }

void timerInit( void )
{
 sa.sa_handler=timerSigHandler;
 sa.sa_flags=SA_RESTART;
 sigemptyset( &sa.sa_mask );
 sigaction( SIGALRM,&sa,NULL );
 it.it_interval.tv_sec=0;
 it.it_interval.tv_usec=20000;
 it.it_value.tv_sec=0;
 it.it_value.tv_usec=50000;
 setitimer( ITIMER_REAL,&it,NULL );
}

void timerDone( void )
{
 it.it_interval.tv_sec=0;
 it.it_interval.tv_usec=0;
 it.it_value.tv_sec=0;
 it.it_value.tv_usec=0;
 setitimer( ITIMER_REAL,&it,NULL );
}

