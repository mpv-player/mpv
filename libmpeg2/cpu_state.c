/*
 * cpu_state.c
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
 *
 * Modified for use with MPlayer, see libmpeg-0.4.1.diff for the exact changes.
 * detailed changelog at http://svn.mplayerhq.hu/mplayer/trunk/
 * $Id$
 */

#include "config.h"

#include <stdlib.h>
#include <inttypes.h>

#include "mpeg2.h"
#include "attributes.h"
#include "mpeg2_internal.h"
#if defined(ARCH_X86) || defined(ARCH_X86_64)
#include "mmx.h"
#endif

void (* mpeg2_cpu_state_save) (cpu_state_t * state) = NULL;
void (* mpeg2_cpu_state_restore) (cpu_state_t * state) = NULL;

#if defined(ARCH_X86) || defined(ARCH_X86_64)
static void state_restore_mmx (cpu_state_t * state)
{
    emms ();
}
#endif

#if defined(ARCH_PPC) && defined(HAVE_ALTIVEC)
#if defined( __APPLE_CC__ ) && defined( __APPLE_ALTIVEC__ )	/* apple */
#define LI(a,b) "li r" #a "," #b "\n\t"
#define STVX0(a,b,c) "stvx v" #a ",0,r" #c "\n\t"
#define STVX(a,b,c) "stvx v" #a ",r" #b ",r" #c "\n\t"
#define LVX0(a,b,c) "lvx v" #a ",0,r" #c "\n\t"
#define LVX(a,b,c) "lvx v" #a ",r" #b ",r" #c "\n\t"
#else			/* gnu */
#define LI(a,b) "li " #a "," #b "\n\t"
#define STVX0(a,b,c) "stvx " #a ",0," #c "\n\t"
#define STVX(a,b,c) "stvx " #a "," #b "," #c "\n\t"
#define LVX0(a,b,c) "lvx " #a ",0," #c "\n\t"
#define LVX(a,b,c) "lvx " #a "," #b "," #c "\n\t"
#endif

static void state_save_altivec (cpu_state_t * state)
{
    asm (LI (9, 16)
	 STVX0 (20, 0, 3)
	 LI (11, 32)
	 STVX (21, 9, 3)
	 LI (9, 48)
	 STVX (22, 11, 3)
	 LI (11, 64)
	 STVX (23, 9, 3)
	 LI (9, 80)
	 STVX (24, 11, 3)
	 LI (11, 96)
	 STVX (25, 9, 3)
	 LI (9, 112)
	 STVX (26, 11, 3)
	 LI (11, 128)
	 STVX (27, 9, 3)
	 LI (9, 144)
	 STVX (28, 11, 3)
	 LI (11, 160)
	 STVX (29, 9, 3)
	 LI (9, 176)
	 STVX (30, 11, 3)
	 STVX (31, 9, 3));
}

static void state_restore_altivec (cpu_state_t * state)
{
    asm (LI (9, 16)
	 LVX0 (20, 0, 3)
	 LI (11, 32)
	 LVX (21, 9, 3)
	 LI (9, 48)
	 LVX (22, 11, 3)
	 LI (11, 64)
	 LVX (23, 9, 3)
	 LI (9, 80)
	 LVX (24, 11, 3)
	 LI (11, 96)
	 LVX (25, 9, 3)
	 LI (9, 112)
	 LVX (26, 11, 3)
	 LI (11, 128)
	 LVX (27, 9, 3)
	 LI (9, 144)
	 LVX (28, 11, 3)
	 LI (11, 160)
	 LVX (29, 9, 3)
	 LI (9, 176)
	 LVX (30, 11, 3)
	 LVX (31, 9, 3));
}
#endif

void mpeg2_cpu_state_init (uint32_t accel)
{
#if defined(ARCH_X86) || defined(ARCH_X86_64)
    if (accel & MPEG2_ACCEL_X86_MMX) {
	mpeg2_cpu_state_restore = state_restore_mmx;
    }
#endif
#if defined(ARCH_PPC) && defined(HAVE_ALTIVEC)
    if (accel & MPEG2_ACCEL_PPC_ALTIVEC) {
	mpeg2_cpu_state_save = state_save_altivec;
	mpeg2_cpu_state_restore = state_restore_altivec;
    }
#endif
}
