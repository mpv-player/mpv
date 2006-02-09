#ifndef __TIMER_H
#define __TIMER_H

extern const char *timer_name;

void InitTimer(void);
unsigned int GetTimer(void);
unsigned int GetTimerMS(void);
//int uGetTimer();
float GetRelativeTime(void);

int usec_sleep(int usec_delay);

/* timer's callback handling */
typedef void timer_callback( void );
extern unsigned set_timer_callback(unsigned ms,timer_callback func);
extern void restore_timer(void);

#endif
