/* This file (C) Mark Zealey <mark@zealos.org> 2002, released under GPL */

#include "geometry.h"
#include "../mp_msg.h"
#include "../mplayer.h" /* exit_player() */
#include <string.h>
#include <stdlib.h> /* strtol */

/* A string of the form xpos[%]:ypos[%] */
char *vo_geometry = NULL;

int geometry_error()
{
	mp_msg(MSGT_VO, MSGL_ERR, "-geometry option format incorrect (%s)\n", vo_geometry);
	exit_player(NULL);		/* ????? what else could we do ? */
	return 0;
}

int get_num(char *s, int *num, char *end)
{
	char *e;
	long int t;

	t = strtol(s, &e, 10);

	if(e != end)
		return 0;

	*num = t;
	return 1;
}

int geometry(int *xpos, int *ypos, int scrw, int scrh, int vidw, int vidh, int fs)
{
	int xper = 0, yper = 0;
	int glen;
	char *colpos;

	*xpos = 0; *ypos = 0;

	if(vo_geometry == NULL)
		return 1;

	glen = strlen(vo_geometry);
	colpos = strchr(vo_geometry, ':');
	if(colpos == NULL)
		colpos = (char *)(vo_geometry + glen);

	/* Parse the x bit */
	if(colpos[-1] == '%') {
		if(!get_num(vo_geometry, &xper, colpos - 1))
			return geometry_error();
	} else {
		if(!get_num(vo_geometry, xpos, colpos))
			return geometry_error();
	}

	if(*colpos != '\0') {
		if(vo_geometry[glen - 1] == '%') {
			if(!get_num(colpos + 1, &yper, vo_geometry + glen - 1))
				return geometry_error();
		} else {
			if(!get_num(colpos + 1, ypos, vo_geometry + glen))
				return geometry_error();
		}
	}

	if(xper)
		*xpos = (scrw - vidw) * ((float)xper / 100.0);
	if(yper)
		*ypos = (scrh - vidh) * ((float)yper / 100.0);

	if(*xpos + vidw > scrw) {
		mp_msg(MSGT_VO, MSGL_ERR, "X position is too large\n");
		return geometry_error();
	}
	if(*ypos + vidh > scrh) {
		mp_msg(MSGT_VO, MSGL_ERR, "Y position is too large\n");
		return geometry_error();
	}

	return 1;
}
