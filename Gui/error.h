
#ifndef _MPLAYER_ERROR_HANDLER
#define _MPLAYER_ERROR_HANDLER

#define True 1
#define False 0

// 0 - standard message
// 1 - detto
// 2 - events
// 3 - skin reader messages
// 4 - bitmap reader messages
// 5 - signal handling messages
// 6 - gtk messages

typedef void (*errorTHandler)( int critical,const char * format, ... );

extern errorTHandler message;
extern errorTHandler dbprintf;

#endif

