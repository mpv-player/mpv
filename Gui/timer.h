
#ifndef __MYTIMER
#define __MYTIMER

typedef void (* timerTSigHandler)( int signum );
extern timerTSigHandler timerSigHandler;

extern void timerSetHandler( timerTSigHandler handler );
extern void timerInit( void );
extern void timerDone( void );

#endif
