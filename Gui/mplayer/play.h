
#ifndef __GUI_PLAY_H
#define __GUI_PLAY_H

#include "../../config.h"

#include "./mplayer.h"

extern void mplMPlayerInit( int argc,char* argv[], char *envp[] );

extern void mplStop();
extern void mplFullScreen( void );
extern void mplPlay( void );
extern void mplPause( void );
extern void mplState( void );
extern void mplResizeToMovieSize( unsigned int width,unsigned int height );

extern void mplIncAudioBufDelay( void );
extern void mplDecAudioBufDelay( void );

extern void  mplRelSeek( float s );
extern void  mplAbsSeek( float s );
extern float mplGetPosition( void );

extern void mplPlayFork( void );
extern void mplSigHandler( int s );
extern void mplPlayerThread( void );

extern void ChangeSkin( char * name );
extern void EventHandling( void );

extern void mplSetFileName( char * fname );

#endif
