/*
 * VIDIX Memory access optimization routines.
 * Copyright (C) 2002 Nick Kurshev
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

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "dha.h"
#include "AsmMacros.h"

#if defined (__i386__) && defined (__NetBSD__)
#include <sys/param.h>
#if __NetBSD_Version__ > 105240000
#include <stdint.h>
#include <stdlib.h>
#include <machine/mtrr.h>
#include <machine/sysarch.h>
#endif
#endif

int	mtrr_set_type(unsigned base,unsigned size,int type)
{
#ifdef __linux__
    FILE * mtrr_fd;
    char * stype;
    switch(type)
    {
	case MTRR_TYPE_UNCACHABLE: stype = "uncachable"; break;
	case MTRR_TYPE_WRCOMB:	   stype = "write-combining"; break;
	case MTRR_TYPE_WRTHROUGH:  stype = "write-through"; break;
	case MTRR_TYPE_WRPROT:	   stype = "write-protect"; break;
	case MTRR_TYPE_WRBACK:	   stype = "write-back"; break;
	default:		   return EINVAL;
    }
    mtrr_fd = fopen("/proc/mtrr","wt");
    if(mtrr_fd)
    {
	char sout[256];
	unsigned wr_len;
	sprintf(sout,"base=0x%08X size=0x%08X type=%s\n",base,size,stype);
	wr_len = fprintf(mtrr_fd,sout);
	/*printf("MTRR: %s\n",sout);*/
	fclose(mtrr_fd);
	return wr_len == strlen(sout) ? 0 : EPERM;
    }
    return ENOSYS;
#elif defined (__i386__ ) && defined (__NetBSD__) && __NetBSD_Version__ > 105240000
    struct mtrr *mtrrp;
    int n;

    mtrrp = malloc(sizeof (struct mtrr));
    mtrrp->base = base;
    mtrrp->len = size;
    mtrrp->type = type;  
    mtrrp->flags = MTRR_VALID | MTRR_PRIVATE;
    n = 1;

    if (i386_set_mtrr(mtrrp, &n) < 0) {
	free(mtrrp);
	return errno;
    }
    free(mtrrp);
    return 0;
#else
    /* NetBSD prior to 1.5Y doesn't have MTRR support */
    return ENOSYS;
#endif
}
