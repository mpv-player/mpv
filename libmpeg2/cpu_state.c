/*
 * cpu_state.c
 * Copyright (C) 2000-2002 Michel Lespinasse <walken@zoy.org>
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

#include "config.h"

#include <stdlib.h>
#include <inttypes.h>

#include "mpeg2.h"
#include "mpeg2_internal.h"
#include "attributes.h"
#ifdef ARCH_X86
#include "mmx.h"
#endif

void (* mpeg2_cpu_state_save) (cpu_state_t * state) = NULL;
void (* mpeg2_cpu_state_restore) (cpu_state_t * state) = NULL;

#ifdef ARCH_X86
static void state_restore_mmx (cpu_state_t * state)
{
    emms ();
}
#endif

#ifdef ARCH_PPC
static void state_save_altivec (cpu_state_t * state)
{
    asm ("						\n"
	"	li		%r9,  16		\n"
	"	stvx		%v20, 0,    %r3		\n"
	"	li		%r11, 32		\n"
	"	stvx		%v21, %r9,  %r3		\n"
	"	li		%r9,  48		\n"
	"	stvx		%v22, %r11, %r3		\n"
	"	li		%r11, 64		\n"
	"	stvx		%v23, %r9,  %r3		\n"
	"	li		%r9,  80		\n"
	"	stvx		%v24, %r11, %r3		\n"
	"	li		%r11, 96		\n"
	"	stvx		%v25, %r9,  %r3		\n"
	"	li		%r9,  112		\n"
	"	stvx		%v26, %r11, %r3		\n"
	"	li		%r11, 128		\n"
	"	stvx		%v27, %r9,  %r3		\n"
	"	li		%r9,  144		\n"
	"	stvx		%v28, %r11, %r3		\n"
	"	li		%r11, 160		\n"
	"	stvx		%v29, %r9,  %r3		\n"
	"	li		%r9,  176		\n"
	"	stvx		%v30, %r11, %r3		\n"
	"	stvx		%v31, %r9,  %r3		\n"
	 );
}

static void state_restore_altivec (cpu_state_t * state)
{
    asm ("						\n"
	"	li		%r9,  16		\n"
	"	lvx		%v20, 0,    %r3		\n"
	"	li		%r11, 32		\n"
	"	lvx		%v21, %r9,  %r3		\n"
	"	li		%r9,  48		\n"
	"	lvx		%v22, %r11, %r3		\n"
	"	li		%r11, 64		\n"
	"	lvx		%v23, %r9,  %r3		\n"
	"	li		%r9,  80		\n"
	"	lvx		%v24, %r11, %r3		\n"
	"	li		%r11, 96		\n"
	"	lvx		%v25, %r9,  %r3		\n"
	"	li		%r9,  112		\n"
	"	lvx		%v26, %r11, %r3		\n"
	"	li		%r11, 128		\n"
	"	lvx		%v27, %r9,  %r3		\n"
	"	li		%r9,  144		\n"
	"	lvx		%v28, %r11, %r3		\n"
	"	li		%r11, 160		\n"
	"	lvx		%v29, %r9,  %r3		\n"
	"	li		%r9,  176		\n"
	"	lvx		%v30, %r11, %r3		\n"
	"	lvx		%v31, %r9,  %r3		\n"
	 );
}
#endif

void mpeg2_cpu_state_init (uint32_t accel)
{
#ifdef ARCH_X86
    if (accel & MPEG2_ACCEL_X86_MMX) {
	mpeg2_cpu_state_restore = state_restore_mmx;
    }
#endif
#ifdef ARCH_PPC
    if (accel & MPEG2_ACCEL_PPC_ALTIVEC) {
	mpeg2_cpu_state_save = state_save_altivec;
	mpeg2_cpu_state_restore = state_restore_altivec;
    }
#endif
}
