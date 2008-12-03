#ifndef MPLAYER_TIMER_H
#define MPLAYER_TIMER_H

extern const char *timer_name;

void InitTimer(void);
unsigned int GetTimer(void);
unsigned int GetTimerMS(void);
//int uGetTimer();
float GetRelativeTime(void);

int usec_sleep(int usec_delay);

/* timer's callback handling */
typedef void timer_callback( void );
unsigned set_timer_callback(unsigned ms,timer_callback func);
void restore_timer(void);

#endif /* MPLAYER_TIMER_H */
