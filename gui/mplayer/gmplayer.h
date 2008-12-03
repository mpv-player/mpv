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

#ifndef MPLAYER_GUI_GMPLAYER_H
#define MPLAYER_GUI_GMPLAYER_H

extern int             mplSubRender;
extern int             mplMainRender;

extern unsigned char * mplDrawBuffer;
extern unsigned char * mplMenuDrawBuffer;
extern int             mainVisible;

extern int             mplMainAutoPlay;
extern int             mplMiddleMenu;

void mplInit( void * disp );

void mplMainDraw( void );
void mplEventHandling( int msg, float param );
void mplMainMouseHandle( int Button, int X, int Y, int RX, int RY );
void mplMainKeyHandle( int KeyCode, int Type, int Key );
void mplDandDHandler(int num, char** files);

void mplSubDraw( void );
void mplSubMouseHandle( int Button, int X, int Y, int RX, int RY );

void mplMenuInit( void );
void mplHideMenu( int mx, int my, int w );
void mplShowMenu( int mx, int my );
void mplMenuMouseHandle( int X, int Y, int RX, int RY );

void mplPBInit( void );
void mplPBShow( int x, int y );

#endif /* MPLAYER_GUI_GMPLAYER_H */
