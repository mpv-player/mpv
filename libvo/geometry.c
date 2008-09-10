/*
 * copyright (C) 2002 Mark Zealey <mark@zealos.org>
 *
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

#include <stdio.h>
#include <string.h>
#include "geometry.h"
#include "mp_msg.h"

/* A string of the form [WxH][+X+Y] or xpos[%]:ypos[%] */
char *vo_geometry = NULL;
// set when either width or height is changed
int geometry_wh_changed = 0;
int geometry_xy_changed = 0;

#define RESET_GEOMETRY width = height = xoff = yoff = xper = yper = -1;

// xpos,ypos: position of the left upper corner
// widw,widh: width and height of the window
// scrw,scrh: width and height of the current screen 
int geometry(int *xpos, int *ypos, int *widw, int *widh, int scrw, int scrh)
{
        int width, height, xoff, yoff, xper, yper;

	width = height = xoff = yoff = xper = yper = -1;

        if(vo_geometry != NULL) {
		if(sscanf(vo_geometry, "%ix%i+%i+%i", &width, &height, &xoff, &yoff) != 4 )
		{
		 RESET_GEOMETRY
		 if(sscanf(vo_geometry, "%ix%i", &width, &height) != 2)
		 {
		  RESET_GEOMETRY
		  if(sscanf(vo_geometry, "+%i+%i", &xoff, &yoff) != 2)
		  {
		   char percent[2];
		   RESET_GEOMETRY
		   if(sscanf(vo_geometry, "%i%%:%i%1[%]", &xper, &yper, percent) != 3)
		   {
		    RESET_GEOMETRY
		    if(sscanf(vo_geometry, "%i:%i%1[%]", &xoff, &yper, percent) != 3)
		    {
		     RESET_GEOMETRY
		     if(sscanf(vo_geometry, "%i%%:%i", &xper, &yoff) != 2)
		     {
		     RESET_GEOMETRY
		     if(sscanf(vo_geometry, "%i:%i", &xoff, &yoff) != 2)
		     {
		      RESET_GEOMETRY
		      if(sscanf(vo_geometry, "%i%1[%]", &xper, percent) != 2)
		      {
			mp_msg(MSGT_VO, MSGL_ERR,
			    "-geometry must be in [WxH][+X+Y] | [X[%%]:[Y[%%]]] format, incorrect (%s)\n", vo_geometry);
			return 0;
		      }
		     }
		    }
		   }
		  }
		 }
		}
	       }

		mp_msg(MSGT_VO, MSGL_V,"geometry set to width: %i,"
		  "height: %i, xoff: %i, yoff: %i, xper: %i, yper: %i\n",
		  width, height, xoff, yoff, xper, yper);
		  
		if(xper >= 0 && xper <= 100) xoff = (scrw - *widw) * ((float)xper / 100.0);
		if(yper >= 0 && yper <= 100) yoff = (scrh - *widh) * ((float)yper / 100.0);

		mp_msg(MSGT_VO, MSGL_V,"geometry set to width: %i,"
		  "height: %i, xoff: %i, yoff: %i, xper: %i, yper: %i\n",
		  width, height, xoff, yoff, xper, yper);
		mp_msg(MSGT_VO, MSGL_V,"geometry window parameter: widw: %i,"
		  " widh: %i, scrw: %i, scrh: %i\n",*widw, *widh, scrw, scrh);
		  
		/* FIXME: better checking of bounds... */
		if( width != -1 && (width < 0 || width > scrw))
		    width = (scrw < *widw) ? scrw : *widw;
		if( height != -1 && (height < 0 || height > scrh))
		    height = (scrh < *widh) ? scrh : *widh;
		if(xoff != -1 && (xoff < 0 || xoff + width > scrw)) xoff = 0;
		if(yoff != -1 && (yoff < 0 || yoff + height > scrh)) yoff = 0;

		if(xoff != -1 && xpos) *xpos = xoff;
		if(yoff != -1 && ypos) *ypos = yoff;
		if(width != -1 && widw) *widw = width;
		if(height != -1 && widh) *widh = height;

		if( width != -1 || height != -1)
		    geometry_wh_changed = 1;
		if( xoff != -1 || yoff != -1)
		    geometry_xy_changed = 1;
        }
	return 1;
}

#undef RESET_GEOMETRY
