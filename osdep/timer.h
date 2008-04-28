#ifndef MPLAYER_TIMER_H
#define MPLAYER_TIMER_H

extern const char timer_name[];

void InitTimer(void);
unsigned int GetTimer(void);
unsigned int GetTimerMS(void);

int usec_sleep(int usec_delay);

#endif /* MPLAYER_TIMER_H */
