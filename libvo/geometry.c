/* This file (C) Mark Zealey <mark@zealos.org> 2002, released under GPL */

#include "geometry.h"
#include "../mp_msg.h"
#include "../mplayer.h" /* exit_player() */
#include <string.h>

/* A string of the form [WxH][+X+Y] or xpos[%]:ypos[%] */
char *vo_geometry = NULL;

int geometry_error()
{
	mp_msg(MSGT_VO, MSGL_ERR, "-geometry must be in [WxH][+X+Y] | [X[%%]:[Y[%%]]] format, incorrect (%s)\n", vo_geometry);
	exit_player(NULL);		/* ????? what else could we do ? */
	return 0;
}

// A little kludge as to not to have to update all drivers
// Only the vo_xv driver supports now the full [WxH][+X+Y] option
int geometryFull(int *pwidth, int *pheight, int *xpos, int *ypos, int scrw, int scrh, int vidw, int vidh)
{
        int width, height, xoff, yoff, xper, yper;

	width = height = xoff = yoff = xper = yper = -1;

	/* no need to save a few extra cpu cycles here ;) */
	/* PUKE i will rewrite this code sometime maybe - euck but it works */
        if(vo_geometry != NULL) {
		if(sscanf(vo_geometry, "%ix%i+%i+%i", &width, &height, &xoff, &yoff) != 4 &&
		   sscanf(vo_geometry, "%ix%i", &width, &height) != 2 &&
		   sscanf(vo_geometry, "+%i+%i", &xoff, &yoff) != 2 &&
		   sscanf(vo_geometry, "%i:%i", &xoff, &yoff) != 2 &&
		   sscanf(vo_geometry, "%i:%i%%", &xper, &yper) != 2 &&
		   sscanf(vo_geometry, "%i%%:%i", &xper, &yper) != 2 &&
		   sscanf(vo_geometry, "%i%%:%i%%", &xper, &yper) != 2 &&
		   sscanf(vo_geometry, "%i%%", &xper) != 1)
			return geometry_error();

		if(xper >= 0 && xper <= 100) xoff = (scrw - vidw) * ((float)xper / 100.0);
		if(yper >= 0 && yper <= 100) yoff = (scrh - vidh) * ((float)yper / 100.0);

		/* FIXME: better checking of bounds... */
		if(width < 0 || width > scrw) width = vidw;
		if(height < 0 || height > scrh) height = vidh;
		if(xoff < 0 || xoff + vidw > scrw) xoff = 0;
		if(yoff < 0 || yoff + vidh > scrh) yoff = 0;

		if(xpos) *xpos = xoff;
		if(ypos) *ypos = yoff;
		if(pwidth) *pwidth = width;
		if(pheight) *pheight = height;
        }
	return 1;
}

// compatibility function
// only libvo working with full geometry options.
int geometry(int *xpos, int *ypos, int scrw, int scrh, int vidw, int vidh)
{
  return geometryFull(NULL, NULL, xpos, ypos, scrw, scrh, vidw, vidh);
}
