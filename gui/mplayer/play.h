/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPLAYER_GUI_PLAY_H
#define MPLAYER_GUI_PLAY_H

#include "config.h"

extern int mplGotoTheNext;

extern void mplEnd( void );
extern void mplFullScreen( void );
extern void mplPlay( void );
extern void mplPause( void );
extern void mplState( void );
extern void mplPrev( void );
extern void mplNext( void );
extern void mplCurr( void );

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

extern void mplSetFileName( char * dir,char * name,int type );

#endif /* MPLAYER_GUI_PLAY_H */
