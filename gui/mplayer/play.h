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

extern int mplGotoTheNext;

void mplEnd( void );
void mplFullScreen( void );
void mplPlay( void );
void mplPause( void );
void mplState( void );
void mplPrev( void );
void mplNext( void );
void mplCurr( void );

void mplIncAudioBufDelay( void );
void mplDecAudioBufDelay( void );

void  mplRelSeek( float s );
void  mplAbsSeek( float s );
float mplGetPosition( void );

void mplPlayFork( void );
void mplSigHandler( int s );
void mplPlayerThread( void );

void ChangeSkin( char * name );
void EventHandling( void );

void mplSetFileName( char * dir, char * name, int type );

#endif /* MPLAYER_GUI_PLAY_H */
