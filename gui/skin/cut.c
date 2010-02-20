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

#include <string.h>
#include <stdlib.h>

#include "cut.h"

void cutItem( char * in,char * out,char sep,int num )
{
 int i,n,c;
 for ( c=0,n=0,i=0;i<strlen( in );i++ )
  {
   if ( in[i] == sep ) n++;
   if ( n >= num && in[i] != sep ) out[c++]=in[i];
   if ( n >= num && in[i+1] == sep ) { out[c]=0; return; }
  }
 out[c]=0;
}

int cutItemToInt( char * in,char sep,int num )
{
 char tmp[512];
 cutItem( in,tmp,sep,num );
 return atoi( tmp );
}

float cutItemToFloat( char * in,char sep,int num )
{
 char tmp[512];
 cutItem( in,tmp,sep,num );
 return atof( tmp );
}

void cutChunk( char * in,char * s1 )
{
 cutItem( in,s1,'=',0 );
 memmove( in,strchr( in,'=' )+1,strlen( in ) - strlen( s1 ) );
}
