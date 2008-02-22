/* This file (C) Mark Zealey <mark@zealos.org 2002, released under GPL */
#ifndef MPLAYER_GEOMETRY_H
#define MPLAYER_GEOMETRY_H

extern char *vo_geometry;
extern int geometry_wh_changed;
extern int geometry_xy_changed;
int geometry(int *xpos, int *ypos, int *widw, int *widh, int scrw, int scrh);

#endif /* MPLAYER_GEOMETRY_H */
