/*
 * alloc.c
 * Copyright (C) 2000-2003 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 * See http://libmpeg2.sourceforge.net/ for updates.
 *
 * mpeg2dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpeg2dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdlib.h>
#include <inttypes.h>

#include "mpeg2.h"

static void * (* malloc_hook) (unsigned size, mpeg2_alloc_t reason) = NULL;
static int (* free_hook) (void * buf) = NULL;

void * mpeg2_malloc (unsigned size, mpeg2_alloc_t reason)
{
    char * buf;

    if (malloc_hook) {
	buf = (char *) malloc_hook (size, reason);
	if (buf)
	    return buf;
    }

    if (size) {
	buf = (char *) malloc (size + 63 + sizeof (void **));
	if (buf) {
	    char * align_buf;

	    align_buf = buf + 63 + sizeof (void **);
	    align_buf -= (long)align_buf & 63;
	    *(((void **)align_buf) - 1) = buf;
	    return align_buf;
	}
    }
    return NULL;
}

void mpeg2_free (void * buf)
{
    if (free_hook && free_hook (buf))
	return;

    if (buf)
	free (*(((void **)buf) - 1));
}

void mpeg2_malloc_hooks (void * malloc (unsigned, mpeg2_alloc_t),
			 int free (void *))
{
    malloc_hook = malloc;
    free_hook = free;
}
