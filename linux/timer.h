#ifndef __TIMER_H
#define __TIMER_H

void InitTimer();
unsigned int GetTimer();
unsigned int GetTimerMS();
//int uGetTimer();
float GetRelativeTime();

int usec_sleep(int usec_delay);

/* timer's callback handling */
typedef void timer_callback( void );
extern unsigned set_timer_callback(unsigned ms,timer_callback func);
extern void restore_timer(void);

#endif
