/* POSIX compatible timer callback */
#include <sys/time.h>
#include <signal.h>
#include <stddef.h>

#include "timer.h"

static timer_callback *user_func = NULL;
static struct itimerval otimer;
static void (*old_alrm)(int) = SIG_DFL;

static void my_alarm_handler( int signo )
{
  if(user_func) (*user_func)();
}

unsigned set_timer_callback(unsigned ms,timer_callback func)
{
   unsigned ret;
   struct itimerval itimer;
   user_func = func;
   getitimer(ITIMER_REAL,&otimer);
   old_alrm = signal(SIGALRM,my_alarm_handler);
   signal(SIGALRM,my_alarm_handler);
   itimer.it_interval.tv_sec = 0;
   itimer.it_interval.tv_usec = ms*1000;
   itimer.it_value.tv_sec = 0;
   itimer.it_value.tv_usec = ms*1000;
   setitimer(ITIMER_REAL,&itimer,NULL);
   getitimer(ITIMER_REAL,&itimer);
   ret = itimer.it_interval.tv_sec*1000 + itimer.it_interval.tv_usec/1000;
   if(!ret) restore_timer();
   return ret;
}

void restore_timer(void)
{
  signal(SIGALRM,old_alrm);
  setitimer(ITIMER_REAL,&otimer,NULL);
}
