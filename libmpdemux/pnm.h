/*
 * Copyright (C) 2002 the xine project
 *
 * This file is part of xine, a free video player.
 *
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id$
 *
 * pnm util functions header by joschka
 */
 
#ifndef HAVE_PNM_H
#define HAVE_PNM_H

#ifndef __CYGWIN__
#include <inttypes.h>
#endif
/*#include "xine_internal.h" */

typedef struct pnm_s pnm_t;

pnm_t*   pnm_connect (int fd,char *url);

int      pnm_read (pnm_t *this, char *data, int len);
void     pnm_close (pnm_t *this);

int      pnm_peek_header (pnm_t *this, char *data);

#endif

