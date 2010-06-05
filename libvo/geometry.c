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
#include <limits.h>
#include "geometry.h"
#include "mp_msg.h"

/* A string of the form [WxH][+X+Y] or xpos[%]:ypos[%] */
char *vo_geometry;
// set when either width or height is changed
int geometry_wh_changed;
int geometry_xy_changed;

// xpos,ypos: position of the left upper corner
// widw,widh: width and height of the window
// scrw,scrh: width and height of the current screen
int geometry(int *xpos, int *ypos, int *widw, int *widh, int scrw, int scrh)
{
        if(vo_geometry != NULL) {
            char xsign[2], ysign[2], dummy[2];
            int width, height, xoff, yoff, xper, yper;
            int i;
            int ok = 0;
            for (i = 0; !ok && i < 9; i++) {
                width = height = xoff = yoff = xper = yper = INT_MIN;
                strcpy(xsign, "+");
                strcpy(ysign, "+");
                switch (i) {
                case 0:
                    ok = sscanf(vo_geometry, "%ix%i%1[+-]%i%1[+-]%i%c",
                                &width, &height, xsign, &xoff, ysign,
                                &yoff, dummy) == 6;
                    break;
                case 1:
                    ok = sscanf(vo_geometry, "%ix%i%c", &width, &height, dummy) == 2;
                    break;
                case 2:
                    ok = sscanf(vo_geometry, "%1[+-]%i%1[+-]%i%c",
                                xsign, &xoff, ysign, &yoff, dummy) == 4;
                    break;
                case 3:
                    ok = sscanf(vo_geometry, "%i%%:%i%1[%]%c", &xper, &yper, dummy, dummy) == 3;
                    break;
                case 4:
                    ok = sscanf(vo_geometry, "%i:%i%1[%]%c", &xoff, &yper, dummy, dummy) == 3;
                    break;
                case 5:
                    ok = sscanf(vo_geometry, "%i%%:%i%c", &xper, &yoff, dummy) == 2;
                    break;
                case 6:
                    ok = sscanf(vo_geometry, "%i:%i%c", &xoff, &yoff, dummy) == 2;
                    break;
                case 7:
                    ok = sscanf(vo_geometry, "%i%1[%]%c", &xper, dummy, dummy) == 2;
                    break;
                case 8:
                    ok = sscanf(vo_geometry, "%i%c", &xoff, dummy) == 1;
                    break;
                }
            }
		      if (!ok) {
			mp_msg(MSGT_VO, MSGL_ERR,
			    "-geometry must be in [WxH][[+-]X[+-]Y] | [X[%%]:[Y[%%]]] format, incorrect (%s)\n", vo_geometry);
			return 0;
		      }

		mp_msg(MSGT_VO, MSGL_V,"geometry window parameter: widw: %i,"
		  " widh: %i, scrw: %i, scrh: %i\n",*widw, *widh, scrw, scrh);

		mp_msg(MSGT_VO, MSGL_V,"geometry set to width: %i,"
		  "height: %i, xoff: %s%i, yoff: %s%i, xper: %i, yper: %i\n",
		  width, height, xsign, xoff, ysign, yoff, xper, yper);

		if (width  > 0 && widw) *widw = width;
		if (height > 0 && widh) *widh = height;

		if(xoff != INT_MIN && xsign[0] == '-') xoff = scrw - *widw - xoff;
		if(yoff != INT_MIN && ysign[0] == '-') yoff = scrh - *widh - yoff;
		if(xper >= 0 && xper <= 100) xoff = (scrw - *widw) * ((float)xper / 100.0);
		if(yper >= 0 && yper <= 100) yoff = (scrh - *widh) * ((float)yper / 100.0);

		mp_msg(MSGT_VO, MSGL_V,"geometry set to width: %i,"
		  "height: %i, xoff: %i, yoff: %i, xper: %i, yper: %i\n",
		  width, height, xoff, yoff, xper, yper);

		if (xoff != INT_MIN && xpos) *xpos = xoff;
		if (yoff != INT_MIN && ypos) *ypos = yoff;

		geometry_wh_changed = width > 0 || height > 0;
		geometry_xy_changed = xoff != INT_MIN || yoff != INT_MIN;
        }
	return 1;
}
