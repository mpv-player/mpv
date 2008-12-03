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

#ifndef MPLAYER_GUI_FONT_H
#define MPLAYER_GUI_FONT_H

#include "gui/bitmap.h"
#include "gui/app.h"

#define fntAlignLeft   0
#define fntAlignCenter 1
#define fntAlignRight  2

typedef struct
{
 int x,y;   // location
 int sx,sy; // size
} fntChar;

typedef struct
{
 fntChar    Fnt[256];
 txSample   Bitmap;
 char       name[128];
} bmpFont;

extern txSample   Bitmap;
extern bmpFont  * Fonts[26];

int  fntAddNewFont( char * name );
void fntFreeFont( void );
int  fntFindID( char * name );
int  fntTextHeight( int id, char * str );
int  fntTextWidth( int id, char * str );

int        fntRead( char * path, char * fname );
txSample * fntRender( wItem * item, int px, const char * fmt, ... );

#endif /* MPLAYER_GUI_FONT_H */
