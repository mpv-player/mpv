/*
 * a52_internal.h
 * Copyright (C) 2000-2001 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of a52dec, a free ATSC A-52 stream decoder.
 * See http://liba52.sourceforge.net/ for updates.
 *
 * a52dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * a52dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define LEVEL_PLUS6DB 2.0
#define LEVEL_PLUS3DB 1.4142135623730951
#define LEVEL_3DB 0.7071067811865476
#define LEVEL_45DB 0.5946035575013605
#define LEVEL_6DB 0.5

#define EXP_REUSE (0)
#define EXP_D15   (1)
#define EXP_D25   (2)
#define EXP_D45   (3)

#define DELTA_BIT_REUSE (0)
#define DELTA_BIT_NEW (1)
#define DELTA_BIT_NONE (2)
#define DELTA_BIT_RESERVED (3)

void bit_allocate (a52_state_t * state, a52_ba_t * ba, int bndstart,
		   int start, int end, int fastleak, int slowleak,
		   uint8_t * exp, int8_t * bap);

int downmix_init (int input, int flags, sample_t * level,
		  sample_t clev, sample_t slev);
int downmix_coeff (sample_t * coeff, int acmod, int output, sample_t level,
		   sample_t clev, sample_t slev);
void downmix (sample_t * samples, int acmod, int output, sample_t bias,
	      sample_t clev, sample_t slev);
void upmix (sample_t * samples, int acmod, int output);

void imdct_init (uint32_t mm_accel);
extern void (* imdct_256) (sample_t * data, sample_t * delay, sample_t bias);
extern void (* imdct_512) (sample_t * data, sample_t * delay, sample_t bias);
void imdct_do_256_mlib (sample_t * data, sample_t * delay, sample_t bias);
void imdct_do_512_mlib (sample_t * data, sample_t * delay, sample_t bias);
