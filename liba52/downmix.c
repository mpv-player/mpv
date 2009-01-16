/*
 * downmix.c
 * Copyright (C) 2000-2002 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of a52dec, a free ATSC A-52 stream decoder.
 * See http://liba52.sourceforge.net/ for updates.
 *
 * Modified for use with MPlayer, changes contained in liba52_changes.diff.
 * detailed changelog at http://svn.mplayerhq.hu/mplayer/trunk/
 * $Id$
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
 *
 * SSE optimizations from Michael Niedermayer (michaelni@gmx.at)
 */

#include "config.h"

#include <string.h>
#include <inttypes.h>

#include "a52.h"
#include "a52_internal.h"
#include "mm_accel.h"

#define CONVERT(acmod,output) (((output) << 3) + (acmod))


void (*a52_downmix)(sample_t * samples, int acmod, int output, sample_t bias,
	      sample_t clev, sample_t slev)= NULL;
void (*a52_upmix)(sample_t * samples, int acmod, int output)= NULL;

static void downmix_SSE (sample_t * samples, int acmod, int output, sample_t bias,
	      sample_t clev, sample_t slev);
static void downmix_3dnow (sample_t * samples, int acmod, int output, sample_t bias,
	      sample_t clev, sample_t slev);
static void downmix_C (sample_t * samples, int acmod, int output, sample_t bias,
	      sample_t clev, sample_t slev);
static void upmix_MMX (sample_t * samples, int acmod, int output);
static void upmix_C (sample_t * samples, int acmod, int output);

void downmix_accel_init(uint32_t mm_accel)
{
    a52_upmix= upmix_C;
    a52_downmix= downmix_C;
#if ARCH_X86 || ARCH_X86_64
    if(mm_accel & MM_ACCEL_X86_MMX) a52_upmix= upmix_MMX;
    if(mm_accel & MM_ACCEL_X86_SSE) a52_downmix= downmix_SSE;
    if(mm_accel & MM_ACCEL_X86_3DNOW) a52_downmix= downmix_3dnow;
#endif
}
 
int a52_downmix_init (int input, int flags, sample_t * level,
		      sample_t clev, sample_t slev)
{
    static uint8_t table[11][8] = {
	{A52_CHANNEL,	A52_DOLBY,	A52_STEREO,	A52_STEREO,
	 A52_STEREO,	A52_STEREO,	A52_STEREO,	A52_STEREO},
	{A52_MONO,	A52_MONO,	A52_MONO,	A52_MONO,
	 A52_MONO,	A52_MONO,	A52_MONO,	A52_MONO},
	{A52_CHANNEL,	A52_DOLBY,	A52_STEREO,	A52_STEREO,
	 A52_STEREO,	A52_STEREO,	A52_STEREO,	A52_STEREO},
	{A52_CHANNEL,	A52_DOLBY,	A52_STEREO,	A52_3F,
	 A52_STEREO,	A52_3F,		A52_STEREO,	A52_3F},
	{A52_CHANNEL,	A52_DOLBY,	A52_STEREO,	A52_STEREO,
	 A52_2F1R,	A52_2F1R,	A52_2F1R,	A52_2F1R},
	{A52_CHANNEL,	A52_DOLBY,	A52_STEREO,	A52_STEREO,
	 A52_2F1R,	A52_3F1R,	A52_2F1R,	A52_3F1R},
	{A52_CHANNEL,	A52_DOLBY,	A52_STEREO,	A52_3F,
	 A52_2F2R,	A52_2F2R,	A52_2F2R,	A52_2F2R},
	{A52_CHANNEL,	A52_DOLBY,	A52_STEREO,	A52_3F,
	 A52_2F2R,	A52_3F2R,	A52_2F2R,	A52_3F2R},
	{A52_CHANNEL1,	A52_MONO,	A52_MONO,	A52_MONO,
	 A52_MONO,	A52_MONO,	A52_MONO,	A52_MONO},
	{A52_CHANNEL2,	A52_MONO,	A52_MONO,	A52_MONO,
	 A52_MONO,	A52_MONO,	A52_MONO,	A52_MONO},
	{A52_CHANNEL,	A52_DOLBY,	A52_STEREO,	A52_DOLBY,
	 A52_DOLBY,	A52_DOLBY,	A52_DOLBY,	A52_DOLBY}
    };
    int output;

    output = flags & A52_CHANNEL_MASK;
    if (output > A52_DOLBY)
	return -1;

    output = table[output][input & 7];

    if ((output == A52_STEREO) &&
	((input == A52_DOLBY) || ((input == A52_3F) && (clev == LEVEL_3DB))))
	output = A52_DOLBY;

    if (flags & A52_ADJUST_LEVEL)
	switch (CONVERT (input & 7, output)) {

	case CONVERT (A52_3F, A52_MONO):
	    *level *= LEVEL_3DB / (1 + clev);
	    break;

	case CONVERT (A52_STEREO, A52_MONO):
	case CONVERT (A52_2F2R, A52_2F1R):
	case CONVERT (A52_3F2R, A52_3F1R):
	level_3db:
	    *level *= LEVEL_3DB;
	    break;

	case CONVERT (A52_3F2R, A52_2F1R):
	    if (clev < LEVEL_PLUS3DB - 1)
		goto level_3db;
	    /* break thru */
	case CONVERT (A52_3F, A52_STEREO):
	case CONVERT (A52_3F1R, A52_2F1R):
	case CONVERT (A52_3F1R, A52_2F2R):
	case CONVERT (A52_3F2R, A52_2F2R):
	    *level /= 1 + clev;
	    break;

	case CONVERT (A52_2F1R, A52_MONO):
	    *level *= LEVEL_PLUS3DB / (2 + slev);
	    break;

	case CONVERT (A52_2F1R, A52_STEREO):
	case CONVERT (A52_3F1R, A52_3F):
	    *level /= 1 + slev * LEVEL_3DB;
	    break;

	case CONVERT (A52_3F1R, A52_MONO):
	    *level *= LEVEL_3DB / (1 + clev + 0.5 * slev);
	    break;

	case CONVERT (A52_3F1R, A52_STEREO):
	    *level /= 1 + clev + slev * LEVEL_3DB;
	    break;

	case CONVERT (A52_2F2R, A52_MONO):
	    *level *= LEVEL_3DB / (1 + slev);
	    break;

	case CONVERT (A52_2F2R, A52_STEREO):
	case CONVERT (A52_3F2R, A52_3F):
	    *level /= 1 + slev;
	    break;

	case CONVERT (A52_3F2R, A52_MONO):
	    *level *= LEVEL_3DB / (1 + clev + slev);
	    break;

	case CONVERT (A52_3F2R, A52_STEREO):
	    *level /= 1 + clev + slev;
	    break;

	case CONVERT (A52_MONO, A52_DOLBY):
	    *level *= LEVEL_PLUS3DB;
	    break;

	case CONVERT (A52_3F, A52_DOLBY):
	case CONVERT (A52_2F1R, A52_DOLBY):
	    *level *= 1 / (1 + LEVEL_3DB);
	    break;

	case CONVERT (A52_3F1R, A52_DOLBY):
	case CONVERT (A52_2F2R, A52_DOLBY):
	    *level *= 1 / (1 + 2 * LEVEL_3DB);
	    break;

	case CONVERT (A52_3F2R, A52_DOLBY):
	    *level *= 1 / (1 + 3 * LEVEL_3DB);
	    break;
	}

    return output;
}

int a52_downmix_coeff (sample_t * coeff, int acmod, int output, sample_t level,
		       sample_t clev, sample_t slev)
{
    switch (CONVERT (acmod, output & A52_CHANNEL_MASK)) {

    case CONVERT (A52_CHANNEL, A52_CHANNEL):
    case CONVERT (A52_MONO, A52_MONO):
    case CONVERT (A52_STEREO, A52_STEREO):
    case CONVERT (A52_3F, A52_3F):
    case CONVERT (A52_2F1R, A52_2F1R):
    case CONVERT (A52_3F1R, A52_3F1R):
    case CONVERT (A52_2F2R, A52_2F2R):
    case CONVERT (A52_3F2R, A52_3F2R):
    case CONVERT (A52_STEREO, A52_DOLBY):
	coeff[0] = coeff[1] = coeff[2] = coeff[3] = coeff[4] = level;
	return 0;

    case CONVERT (A52_CHANNEL, A52_MONO):
	coeff[0] = coeff[1] = level * LEVEL_6DB;
	return 3;

    case CONVERT (A52_STEREO, A52_MONO):
	coeff[0] = coeff[1] = level * LEVEL_3DB;
	return 3;

    case CONVERT (A52_3F, A52_MONO):
	coeff[0] = coeff[2] = level * LEVEL_3DB;
	coeff[1] = level * clev * LEVEL_PLUS3DB;
	return 7;

    case CONVERT (A52_2F1R, A52_MONO):
	coeff[0] = coeff[1] = level * LEVEL_3DB;
	coeff[2] = level * slev * LEVEL_3DB;
	return 7;

    case CONVERT (A52_2F2R, A52_MONO):
	coeff[0] = coeff[1] = level * LEVEL_3DB;
	coeff[2] = coeff[3] = level * slev * LEVEL_3DB;
	return 15;

    case CONVERT (A52_3F1R, A52_MONO):
	coeff[0] = coeff[2] = level * LEVEL_3DB;
	coeff[1] = level * clev * LEVEL_PLUS3DB;
	coeff[3] = level * slev * LEVEL_3DB;
	return 15;

    case CONVERT (A52_3F2R, A52_MONO):
	coeff[0] = coeff[2] = level * LEVEL_3DB;
	coeff[1] = level * clev * LEVEL_PLUS3DB;
	coeff[3] = coeff[4] = level * slev * LEVEL_3DB;
	return 31;

    case CONVERT (A52_MONO, A52_DOLBY):
	coeff[0] = level * LEVEL_3DB;
	return 0;

    case CONVERT (A52_3F, A52_DOLBY):
	clev = LEVEL_3DB;
    case CONVERT (A52_3F, A52_STEREO):
    case CONVERT (A52_3F1R, A52_2F1R):
    case CONVERT (A52_3F2R, A52_2F2R):
	coeff[0] = coeff[2] = coeff[3] = coeff[4] = level;
	coeff[1] = level * clev;
	return 7;

    case CONVERT (A52_2F1R, A52_DOLBY):
	slev = 1;
    case CONVERT (A52_2F1R, A52_STEREO):
	coeff[0] = coeff[1] = level;
	coeff[2] = level * slev * LEVEL_3DB;
	return 7;

    case CONVERT (A52_3F1R, A52_DOLBY):
	clev = LEVEL_3DB;
	slev = 1;
    case CONVERT (A52_3F1R, A52_STEREO):
	coeff[0] = coeff[2] = level;
	coeff[1] = level * clev;
	coeff[3] = level * slev * LEVEL_3DB;
	return 15;

    case CONVERT (A52_2F2R, A52_DOLBY):
	slev = LEVEL_3DB;
    case CONVERT (A52_2F2R, A52_STEREO):
	coeff[0] = coeff[1] = level;
	coeff[2] = coeff[3] = level * slev;
	return 15;

    case CONVERT (A52_3F2R, A52_DOLBY):
	clev = LEVEL_3DB;
    case CONVERT (A52_3F2R, A52_2F1R):
	slev = LEVEL_3DB;
    case CONVERT (A52_3F2R, A52_STEREO):
	coeff[0] = coeff[2] = level;
	coeff[1] = level * clev;
	coeff[3] = coeff[4] = level * slev;
	return 31;

    case CONVERT (A52_3F1R, A52_3F):
	coeff[0] = coeff[1] = coeff[2] = level;
	coeff[3] = level * slev * LEVEL_3DB;
	return 13;

    case CONVERT (A52_3F2R, A52_3F):
	coeff[0] = coeff[1] = coeff[2] = level;
	coeff[3] = coeff[4] = level * slev;
	return 29;

    case CONVERT (A52_2F2R, A52_2F1R):
	coeff[0] = coeff[1] = level;
	coeff[2] = coeff[3] = level * LEVEL_3DB;
	return 12;

    case CONVERT (A52_3F2R, A52_3F1R):
	coeff[0] = coeff[1] = coeff[2] = level;
	coeff[3] = coeff[4] = level * LEVEL_3DB;
	return 24;

    case CONVERT (A52_2F1R, A52_2F2R):
	coeff[0] = coeff[1] = level;
	coeff[2] = level * LEVEL_3DB;
	return 0;

    case CONVERT (A52_3F1R, A52_2F2R):
	coeff[0] = coeff[2] = level;
	coeff[1] = level * clev;
	coeff[3] = level * LEVEL_3DB;
	return 7;

    case CONVERT (A52_3F1R, A52_3F2R):
	coeff[0] = coeff[1] = coeff[2] = level;
	coeff[3] = level * LEVEL_3DB;
	return 0;

    case CONVERT (A52_CHANNEL, A52_CHANNEL1):
	coeff[0] = level;
	coeff[1] = 0;
	return 0;

    case CONVERT (A52_CHANNEL, A52_CHANNEL2):
	coeff[0] = 0;
	coeff[1] = level;
	return 0;
    }

    return -1;	/* NOTREACHED */
}

static void mix2to1 (sample_t * dest, sample_t * src, sample_t bias)
{
    int i;

    for (i = 0; i < 256; i++)
	dest[i] += src[i] + bias;
}

static void mix3to1 (sample_t * samples, sample_t bias)
{
    int i;

    for (i = 0; i < 256; i++)
	samples[i] += samples[i + 256] + samples[i + 512] + bias;
}

static void mix4to1 (sample_t * samples, sample_t bias)
{
    int i;

    for (i = 0; i < 256; i++)
	samples[i] += (samples[i + 256] + samples[i + 512] +
		       samples[i + 768] + bias);
}

static void mix5to1 (sample_t * samples, sample_t bias)
{
    int i;

    for (i = 0; i < 256; i++)
	samples[i] += (samples[i + 256] + samples[i + 512] +
		       samples[i + 768] + samples[i + 1024] + bias);
}

static void mix3to2 (sample_t * samples, sample_t bias)
{
    int i;
    sample_t common;

    for (i = 0; i < 256; i++) {
	common = samples[i + 256] + bias;
	samples[i] += common;
	samples[i + 256] = samples[i + 512] + common;
    }
}

static void mix21to2 (sample_t * left, sample_t * right, sample_t bias)
{
    int i;
    sample_t common;

    for (i = 0; i < 256; i++) {
	common = right[i + 256] + bias;
	left[i] += common;
	right[i] += common;
    }
}

static void mix21toS (sample_t * samples, sample_t bias)
{
    int i;
    sample_t surround;

    for (i = 0; i < 256; i++) {
	surround = samples[i + 512];
	samples[i] += bias - surround;
	samples[i + 256] += bias + surround;
    }
}

static void mix31to2 (sample_t * samples, sample_t bias)
{
    int i;
    sample_t common;

    for (i = 0; i < 256; i++) {
	common = samples[i + 256] + samples[i + 768] + bias;
	samples[i] += common;
	samples[i + 256] = samples[i + 512] + common;
    }
}

static void mix31toS (sample_t * samples, sample_t bias)
{
    int i;
    sample_t common, surround;

    for (i = 0; i < 256; i++) {
	common = samples[i + 256] + bias;
	surround = samples[i + 768];
	samples[i] += common - surround;
	samples[i + 256] = samples[i + 512] + common + surround;
    }
}

static void mix22toS (sample_t * samples, sample_t bias)
{
    int i;
    sample_t surround;

    for (i = 0; i < 256; i++) {
	surround = samples[i + 512] + samples[i + 768];
	samples[i] += bias - surround;
	samples[i + 256] += bias + surround;
    }
}

static void mix32to2 (sample_t * samples, sample_t bias)
{
    int i;
    sample_t common;

    for (i = 0; i < 256; i++) {
	common = samples[i + 256] + bias;
	samples[i] += common + samples[i + 768];
	samples[i + 256] = common + samples[i + 512] + samples[i + 1024];
    }
}

static void mix32toS (sample_t * samples, sample_t bias)
{
    int i;
    sample_t common, surround;

    for (i = 0; i < 256; i++) {
	common = samples[i + 256] + bias;
	surround = samples[i + 768] + samples[i + 1024];
	samples[i] += common - surround;
	samples[i + 256] = samples[i + 512] + common + surround;
    }
}

static void move2to1 (sample_t * src, sample_t * dest, sample_t bias)
{
    int i;

    for (i = 0; i < 256; i++)
	dest[i] = src[i] + src[i + 256] + bias;
}

static void zero (sample_t * samples)
{
    int i;

    for (i = 0; i < 256; i++)
	samples[i] = 0;
}

void downmix_C (sample_t * samples, int acmod, int output, sample_t bias,
		  sample_t clev, sample_t slev)
{
    switch (CONVERT (acmod, output & A52_CHANNEL_MASK)) {

    case CONVERT (A52_CHANNEL, A52_CHANNEL2):
	memcpy (samples, samples + 256, 256 * sizeof (sample_t));
	break;

    case CONVERT (A52_CHANNEL, A52_MONO):
    case CONVERT (A52_STEREO, A52_MONO):
    mix_2to1:
	mix2to1 (samples, samples + 256, bias);
	break;

    case CONVERT (A52_2F1R, A52_MONO):
	if (slev == 0)
	    goto mix_2to1;
    case CONVERT (A52_3F, A52_MONO):
    mix_3to1:
	mix3to1 (samples, bias);
	break;

    case CONVERT (A52_3F1R, A52_MONO):
	if (slev == 0)
	    goto mix_3to1;
    case CONVERT (A52_2F2R, A52_MONO):
	if (slev == 0)
	    goto mix_2to1;
	mix4to1 (samples, bias);
	break;

    case CONVERT (A52_3F2R, A52_MONO):
	if (slev == 0)
	    goto mix_3to1;
	mix5to1 (samples, bias);
	break;

    case CONVERT (A52_MONO, A52_DOLBY):
	memcpy (samples + 256, samples, 256 * sizeof (sample_t));
	break;

    case CONVERT (A52_3F, A52_STEREO):
    case CONVERT (A52_3F, A52_DOLBY):
    mix_3to2:
	mix3to2 (samples, bias);
	break;

    case CONVERT (A52_2F1R, A52_STEREO):
	if (slev == 0)
	    break;
	mix21to2 (samples, samples + 256, bias);
	break;

    case CONVERT (A52_2F1R, A52_DOLBY):
	mix21toS (samples, bias);
	break;

    case CONVERT (A52_3F1R, A52_STEREO):
	if (slev == 0)
	    goto mix_3to2;
	mix31to2 (samples, bias);
	break;

    case CONVERT (A52_3F1R, A52_DOLBY):
	mix31toS (samples, bias);
	break;

    case CONVERT (A52_2F2R, A52_STEREO):
	if (slev == 0)
	    break;
	mix2to1 (samples, samples + 512, bias);
	mix2to1 (samples + 256, samples + 768, bias);
	break;

    case CONVERT (A52_2F2R, A52_DOLBY):
	mix22toS (samples, bias);
	break;

    case CONVERT (A52_3F2R, A52_STEREO):
	if (slev == 0)
	    goto mix_3to2;
	mix32to2 (samples, bias);
	break;

    case CONVERT (A52_3F2R, A52_DOLBY):
	mix32toS (samples, bias);
	break;

    case CONVERT (A52_3F1R, A52_3F):
	if (slev == 0)
	    break;
	mix21to2 (samples, samples + 512, bias);
	break;

    case CONVERT (A52_3F2R, A52_3F):
	if (slev == 0)
	    break;
	mix2to1 (samples, samples + 768, bias);
	mix2to1 (samples + 512, samples + 1024, bias);
	break;

    case CONVERT (A52_3F1R, A52_2F1R):
	mix3to2 (samples, bias);
	memcpy (samples + 512, samples + 768, 256 * sizeof (sample_t));
	break;

    case CONVERT (A52_2F2R, A52_2F1R):
	mix2to1 (samples + 512, samples + 768, bias);
	break;

    case CONVERT (A52_3F2R, A52_2F1R):
	mix3to2 (samples, bias); //FIXME possible bug? (output doesnt seem to be used)
	move2to1 (samples + 768, samples + 512, bias);
	break;

    case CONVERT (A52_3F2R, A52_3F1R):
	mix2to1 (samples + 768, samples + 1024, bias);
	break;

    case CONVERT (A52_2F1R, A52_2F2R):
	memcpy (samples + 768, samples + 512, 256 * sizeof (sample_t));
	break;

    case CONVERT (A52_3F1R, A52_2F2R):
	mix3to2 (samples, bias);
	memcpy (samples + 512, samples + 768, 256 * sizeof (sample_t));
	break;

    case CONVERT (A52_3F2R, A52_2F2R):
	mix3to2 (samples, bias);
	memcpy (samples + 512, samples + 768, 256 * sizeof (sample_t));
	memcpy (samples + 768, samples + 1024, 256 * sizeof (sample_t));
	break;

    case CONVERT (A52_3F1R, A52_3F2R):
	memcpy (samples + 1024, samples + 768, 256 * sizeof (sample_t));
	break;
    }
}

void upmix_C (sample_t * samples, int acmod, int output)
{
    switch (CONVERT (acmod, output & A52_CHANNEL_MASK)) {

    case CONVERT (A52_CHANNEL, A52_CHANNEL2):
	memcpy (samples + 256, samples, 256 * sizeof (sample_t));
	break;

    case CONVERT (A52_3F2R, A52_MONO):
	zero (samples + 1024);
    case CONVERT (A52_3F1R, A52_MONO):
    case CONVERT (A52_2F2R, A52_MONO):
	zero (samples + 768);
    case CONVERT (A52_3F, A52_MONO):
    case CONVERT (A52_2F1R, A52_MONO):
	zero (samples + 512);
    case CONVERT (A52_CHANNEL, A52_MONO):
    case CONVERT (A52_STEREO, A52_MONO):
	zero (samples + 256);
	break;

    case CONVERT (A52_3F2R, A52_STEREO):
    case CONVERT (A52_3F2R, A52_DOLBY):
	zero (samples + 1024);
    case CONVERT (A52_3F1R, A52_STEREO):
    case CONVERT (A52_3F1R, A52_DOLBY):
	zero (samples + 768);
    case CONVERT (A52_3F, A52_STEREO):
    case CONVERT (A52_3F, A52_DOLBY):
    mix_3to2:
	memcpy (samples + 512, samples + 256, 256 * sizeof (sample_t));
	zero (samples + 256);
	break;

    case CONVERT (A52_2F2R, A52_STEREO):
    case CONVERT (A52_2F2R, A52_DOLBY):
	zero (samples + 768);
    case CONVERT (A52_2F1R, A52_STEREO):
    case CONVERT (A52_2F1R, A52_DOLBY):
	zero (samples + 512);
	break;

    case CONVERT (A52_3F2R, A52_3F):
	zero (samples + 1024);
    case CONVERT (A52_3F1R, A52_3F):
    case CONVERT (A52_2F2R, A52_2F1R):
	zero (samples + 768);
	break;

    case CONVERT (A52_3F2R, A52_3F1R):
	zero (samples + 1024);
	break;

    case CONVERT (A52_3F2R, A52_2F1R):
	zero (samples + 1024);
    case CONVERT (A52_3F1R, A52_2F1R):
    mix_31to21:
	memcpy (samples + 768, samples + 512, 256 * sizeof (sample_t));
	goto mix_3to2;

    case CONVERT (A52_3F2R, A52_2F2R):
	memcpy (samples + 1024, samples + 768, 256 * sizeof (sample_t));
	goto mix_31to21;
    }
}

#if ARCH_X86 || ARCH_X86_64
static void mix2to1_SSE (sample_t * dest, sample_t * src, sample_t bias)
{
	__asm__ volatile(
	"movlps %2, %%xmm7		\n\t"
	"shufps $0x00, %%xmm7, %%xmm7	\n\t"
	"mov $-1024, %%"REG_S"		\n\t"
	ASMALIGN(4)
	"1:				\n\t"
	"movaps (%0, %%"REG_S"), %%xmm0	\n\t" 
	"movaps 16(%0, %%"REG_S"), %%xmm1\n\t" 
	"addps (%1, %%"REG_S"), %%xmm0	\n\t" 
	"addps 16(%1, %%"REG_S"), %%xmm1\n\t" 
	"addps %%xmm7, %%xmm0		\n\t"
	"addps %%xmm7, %%xmm1		\n\t"
	"movaps %%xmm0, (%1, %%"REG_S")	\n\t"
	"movaps %%xmm1, 16(%1, %%"REG_S")\n\t"
	"add $32, %%"REG_S"		\n\t"
	" jnz 1b			\n\t"
	:: "r" (src+256), "r" (dest+256), "m" (bias)
	: "%"REG_S
	);
}

static void mix3to1_SSE (sample_t * samples, sample_t bias)
{
	__asm__ volatile(
	"movlps %1, %%xmm7		\n\t"
	"shufps $0x00, %%xmm7, %%xmm7	\n\t"
	"mov $-1024, %%"REG_S"		\n\t"
	ASMALIGN(4)
	"1:				\n\t"
	"movaps (%0, %%"REG_S"), %%xmm0	\n\t" 
	"movaps 1024(%0, %%"REG_S"), %%xmm1\n\t" 
	"addps 2048(%0, %%"REG_S"), %%xmm0\n\t" 
	"addps %%xmm7, %%xmm1		\n\t"
	"addps %%xmm1, %%xmm0		\n\t"
	"movaps %%xmm0, (%0, %%"REG_S")	\n\t"
	"add $16, %%"REG_S"		\n\t"
	" jnz 1b			\n\t"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S
	);
}

static void mix4to1_SSE (sample_t * samples, sample_t bias)
{
	__asm__ volatile(
	"movlps %1, %%xmm7		\n\t"
	"shufps $0x00, %%xmm7, %%xmm7	\n\t"
	"mov $-1024, %%"REG_S"		\n\t"
	ASMALIGN(4)
	"1:				\n\t"
	"movaps (%0, %%"REG_S"), %%xmm0	\n\t" 
	"movaps 1024(%0, %%"REG_S"), %%xmm1\n\t" 
	"addps 2048(%0, %%"REG_S"), %%xmm0\n\t" 
	"addps 3072(%0, %%"REG_S"), %%xmm1\n\t" 
	"addps %%xmm7, %%xmm0		\n\t"
	"addps %%xmm1, %%xmm0		\n\t"
	"movaps %%xmm0, (%0, %%"REG_S")	\n\t"
	"add $16, %%"REG_S"		\n\t"
	" jnz 1b			\n\t"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S
	);
}

static void mix5to1_SSE (sample_t * samples, sample_t bias)
{
	__asm__ volatile(
	"movlps %1, %%xmm7		\n\t"
	"shufps $0x00, %%xmm7, %%xmm7	\n\t"
	"mov $-1024, %%"REG_S"		\n\t"
	ASMALIGN(4)
	"1:				\n\t"
	"movaps (%0, %%"REG_S"), %%xmm0	\n\t" 
	"movaps 1024(%0, %%"REG_S"), %%xmm1\n\t" 
	"addps 2048(%0, %%"REG_S"), %%xmm0\n\t" 
	"addps 3072(%0, %%"REG_S"), %%xmm1\n\t" 
	"addps %%xmm7, %%xmm0		\n\t"
	"addps 4096(%0, %%"REG_S"), %%xmm1\n\t" 
	"addps %%xmm1, %%xmm0		\n\t"
	"movaps %%xmm0, (%0, %%"REG_S")	\n\t"
	"add $16, %%"REG_S"		\n\t"
	" jnz 1b			\n\t"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S
	);
}

static void mix3to2_SSE (sample_t * samples, sample_t bias)
{
	__asm__ volatile(
	"movlps %1, %%xmm7		\n\t"
	"shufps $0x00, %%xmm7, %%xmm7	\n\t"
	"mov $-1024, %%"REG_S"		\n\t"
	ASMALIGN(4)
	"1:				\n\t"
	"movaps 1024(%0, %%"REG_S"), %%xmm0\n\t" 
	"addps %%xmm7, %%xmm0		\n\t" //common
	"movaps (%0, %%"REG_S"), %%xmm1	\n\t" 
	"movaps 2048(%0, %%"REG_S"), %%xmm2\n\t"
	"addps %%xmm0, %%xmm1		\n\t"
	"addps %%xmm0, %%xmm2		\n\t"
	"movaps %%xmm1, (%0, %%"REG_S")	\n\t"
	"movaps %%xmm2, 1024(%0, %%"REG_S")\n\t"
	"add $16, %%"REG_S"		\n\t"
	" jnz 1b			\n\t"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S
	);
}

static void mix21to2_SSE (sample_t * left, sample_t * right, sample_t bias)
{
	__asm__ volatile(
		"movlps %2, %%xmm7		\n\t"
		"shufps $0x00, %%xmm7, %%xmm7	\n\t"
		"mov $-1024, %%"REG_S"		\n\t"
		ASMALIGN(4)
		"1:				\n\t"
		"movaps 1024(%1, %%"REG_S"), %%xmm0\n\t" 
		"addps %%xmm7, %%xmm0		\n\t" //common
		"movaps (%0, %%"REG_S"), %%xmm1	\n\t" 
		"movaps (%1, %%"REG_S"), %%xmm2	\n\t"
		"addps %%xmm0, %%xmm1		\n\t"
		"addps %%xmm0, %%xmm2		\n\t"
		"movaps %%xmm1, (%0, %%"REG_S")	\n\t"
		"movaps %%xmm2, (%1, %%"REG_S")	\n\t"
		"add $16, %%"REG_S"		\n\t"
		" jnz 1b			\n\t"
	:: "r" (left+256), "r" (right+256), "m" (bias)
	: "%"REG_S
	);
}

static void mix21toS_SSE (sample_t * samples, sample_t bias)
{
	__asm__ volatile(
		"movlps %1, %%xmm7		\n\t"
		"shufps $0x00, %%xmm7, %%xmm7	\n\t"
		"mov $-1024, %%"REG_S"		\n\t"
		ASMALIGN(4)
		"1:				\n\t"
		"movaps 2048(%0, %%"REG_S"), %%xmm0\n\t"  // surround
		"movaps (%0, %%"REG_S"), %%xmm1	\n\t" 
		"movaps 1024(%0, %%"REG_S"), %%xmm2\n\t"
		"addps %%xmm7, %%xmm1		\n\t"
		"addps %%xmm7, %%xmm2		\n\t"
		"subps %%xmm0, %%xmm1		\n\t"
		"addps %%xmm0, %%xmm2		\n\t"
		"movaps %%xmm1, (%0, %%"REG_S")	\n\t"
		"movaps %%xmm2, 1024(%0, %%"REG_S")\n\t"
		"add $16, %%"REG_S"		\n\t"
		" jnz 1b			\n\t"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S
	);
}

static void mix31to2_SSE (sample_t * samples, sample_t bias)
{
	__asm__ volatile(
		"movlps %1, %%xmm7		\n\t"
		"shufps $0x00, %%xmm7, %%xmm7	\n\t"
		"mov $-1024, %%"REG_S"		\n\t"
		ASMALIGN(4)
		"1:				\n\t"
		"movaps 1024(%0, %%"REG_S"), %%xmm0\n\t"  
		"addps 3072(%0, %%"REG_S"), %%xmm0\n\t"  
		"addps %%xmm7, %%xmm0		\n\t" // common
		"movaps (%0, %%"REG_S"), %%xmm1	\n\t" 
		"movaps 2048(%0, %%"REG_S"), %%xmm2\n\t"
		"addps %%xmm0, %%xmm1		\n\t"
		"addps %%xmm0, %%xmm2		\n\t"
		"movaps %%xmm1, (%0, %%"REG_S")	\n\t"
		"movaps %%xmm2, 1024(%0, %%"REG_S")\n\t"
		"add $16, %%"REG_S"		\n\t"
		" jnz 1b			\n\t"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S
	);
}

static void mix31toS_SSE (sample_t * samples, sample_t bias)
{
	__asm__ volatile(
		"movlps %1, %%xmm7		\n\t"
		"shufps $0x00, %%xmm7, %%xmm7	\n\t"
		"mov $-1024, %%"REG_S"		\n\t"
		ASMALIGN(4)
		"1:				\n\t"
		"movaps 1024(%0, %%"REG_S"), %%xmm0\n\t"  
		"movaps 3072(%0, %%"REG_S"), %%xmm3\n\t" // surround
		"addps %%xmm7, %%xmm0		\n\t" // common
		"movaps (%0, %%"REG_S"), %%xmm1	\n\t" 
		"movaps 2048(%0, %%"REG_S"), %%xmm2\n\t"
		"addps %%xmm0, %%xmm1		\n\t"
		"addps %%xmm0, %%xmm2		\n\t"
		"subps %%xmm3, %%xmm1		\n\t"
		"addps %%xmm3, %%xmm2		\n\t"
		"movaps %%xmm1, (%0, %%"REG_S")	\n\t"
		"movaps %%xmm2, 1024(%0, %%"REG_S")\n\t"
		"add $16, %%"REG_S"		\n\t"
		" jnz 1b			\n\t"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S
	);
}

static void mix22toS_SSE (sample_t * samples, sample_t bias)
{
	__asm__ volatile(
		"movlps %1, %%xmm7		\n\t"
		"shufps $0x00, %%xmm7, %%xmm7	\n\t"
		"mov $-1024, %%"REG_S"		\n\t"
		ASMALIGN(4)
		"1:				\n\t"
		"movaps 2048(%0, %%"REG_S"), %%xmm0\n\t"  
		"addps 3072(%0, %%"REG_S"), %%xmm0\n\t" // surround
		"movaps (%0, %%"REG_S"), %%xmm1	\n\t" 
		"movaps 1024(%0, %%"REG_S"), %%xmm2\n\t"
		"addps %%xmm7, %%xmm1		\n\t"
		"addps %%xmm7, %%xmm2		\n\t"
		"subps %%xmm0, %%xmm1		\n\t"
		"addps %%xmm0, %%xmm2		\n\t"
		"movaps %%xmm1, (%0, %%"REG_S")	\n\t"
		"movaps %%xmm2, 1024(%0, %%"REG_S")\n\t"
		"add $16, %%"REG_S"		\n\t"
		" jnz 1b			\n\t"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S
	);
}

static void mix32to2_SSE (sample_t * samples, sample_t bias)
{
	__asm__ volatile(
	"movlps %1, %%xmm7		\n\t"
	"shufps $0x00, %%xmm7, %%xmm7	\n\t"
	"mov $-1024, %%"REG_S"		\n\t"
	ASMALIGN(4)
	"1:				\n\t"
	"movaps 1024(%0, %%"REG_S"), %%xmm0\n\t" 
	"addps %%xmm7, %%xmm0		\n\t" // common
	"movaps %%xmm0, %%xmm1		\n\t" // common
	"addps (%0, %%"REG_S"), %%xmm0	\n\t" 
	"addps 2048(%0, %%"REG_S"), %%xmm1\n\t" 
	"addps 3072(%0, %%"REG_S"), %%xmm0\n\t" 
	"addps 4096(%0, %%"REG_S"), %%xmm1\n\t" 
	"movaps %%xmm0, (%0, %%"REG_S")	\n\t"
	"movaps %%xmm1, 1024(%0, %%"REG_S")\n\t"
	"add $16, %%"REG_S"		\n\t"
	" jnz 1b			\n\t"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S
	);
}

static void mix32toS_SSE (sample_t * samples, sample_t bias)
{
	__asm__ volatile(
	"movlps %1, %%xmm7		\n\t"
	"shufps $0x00, %%xmm7, %%xmm7	\n\t"
	"mov $-1024, %%"REG_S"		\n\t"
	ASMALIGN(4)
	"1:				\n\t"
	"movaps 1024(%0, %%"REG_S"), %%xmm0\n\t" 
	"movaps 3072(%0, %%"REG_S"), %%xmm2\n\t" 
	"addps %%xmm7, %%xmm0		\n\t" // common
	"addps 4096(%0, %%"REG_S"), %%xmm2\n\t" // surround	
	"movaps (%0, %%"REG_S"), %%xmm1	\n\t" 
	"movaps 2048(%0, %%"REG_S"), %%xmm3\n\t" 
	"subps %%xmm2, %%xmm1		\n\t"	
	"addps %%xmm2, %%xmm3		\n\t"	
	"addps %%xmm0, %%xmm1		\n\t"	
	"addps %%xmm0, %%xmm3		\n\t"	
	"movaps %%xmm1, (%0, %%"REG_S")	\n\t"
	"movaps %%xmm3, 1024(%0, %%"REG_S")\n\t"
	"add $16, %%"REG_S"		\n\t"
	" jnz 1b			\n\t"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S
	);
}

static void move2to1_SSE (sample_t * src, sample_t * dest, sample_t bias)
{
	__asm__ volatile(
		"movlps %2, %%xmm7		\n\t"
		"shufps $0x00, %%xmm7, %%xmm7	\n\t"
		"mov $-1024, %%"REG_S"		\n\t"
		ASMALIGN(4)
		"1:				\n\t"
		"movaps (%0, %%"REG_S"), %%xmm0	\n\t"  
		"movaps 16(%0, %%"REG_S"), %%xmm1\n\t"  
		"addps 1024(%0, %%"REG_S"), %%xmm0\n\t"
		"addps 1040(%0, %%"REG_S"), %%xmm1\n\t"
		"addps %%xmm7, %%xmm0		\n\t"
		"addps %%xmm7, %%xmm1		\n\t"
		"movaps %%xmm0, (%1, %%"REG_S")	\n\t"
		"movaps %%xmm1, 16(%1, %%"REG_S")\n\t"
		"add $32, %%"REG_S"		\n\t"
		" jnz 1b			\n\t"
	:: "r" (src+256), "r" (dest+256), "m" (bias)
	: "%"REG_S
	);
}

static void zero_MMX(sample_t * samples)
{
	__asm__ volatile(
		"mov $-1024, %%"REG_S"		\n\t"
		"pxor %%mm0, %%mm0		\n\t"
		ASMALIGN(4)
		"1:				\n\t"
		"movq %%mm0, (%0, %%"REG_S")	\n\t"
		"movq %%mm0, 8(%0, %%"REG_S")	\n\t"
		"movq %%mm0, 16(%0, %%"REG_S")	\n\t"
		"movq %%mm0, 24(%0, %%"REG_S")	\n\t"
		"add $32, %%"REG_S"		\n\t"
		" jnz 1b			\n\t"
		"emms"
	:: "r" (samples+256)
	: "%"REG_S
	);
}

static void downmix_SSE (sample_t * samples, int acmod, int output, sample_t bias,
	      sample_t clev, sample_t slev)
{
    switch (CONVERT (acmod, output & A52_CHANNEL_MASK)) {

    case CONVERT (A52_CHANNEL, A52_CHANNEL2):
	memcpy (samples, samples + 256, 256 * sizeof (sample_t));
	break;

    case CONVERT (A52_CHANNEL, A52_MONO):
    case CONVERT (A52_STEREO, A52_MONO):
    mix_2to1_SSE:
	mix2to1_SSE (samples, samples + 256, bias);
	break;

    case CONVERT (A52_2F1R, A52_MONO):
	if (slev == 0)
	    goto mix_2to1_SSE;
    case CONVERT (A52_3F, A52_MONO):
    mix_3to1_SSE:
	mix3to1_SSE (samples, bias);
	break;

    case CONVERT (A52_3F1R, A52_MONO):
	if (slev == 0)
	    goto mix_3to1_SSE;
    case CONVERT (A52_2F2R, A52_MONO):
	if (slev == 0)
	    goto mix_2to1_SSE;
	mix4to1_SSE (samples, bias);
	break;

    case CONVERT (A52_3F2R, A52_MONO):
	if (slev == 0)
	    goto mix_3to1_SSE;
	mix5to1_SSE (samples, bias);
	break;

    case CONVERT (A52_MONO, A52_DOLBY):
	memcpy (samples + 256, samples, 256 * sizeof (sample_t));
	break;

    case CONVERT (A52_3F, A52_STEREO):
    case CONVERT (A52_3F, A52_DOLBY):
    mix_3to2_SSE:
	mix3to2_SSE (samples, bias);
	break;

    case CONVERT (A52_2F1R, A52_STEREO):
	if (slev == 0)
	    break;
	mix21to2_SSE (samples, samples + 256, bias);
	break;

    case CONVERT (A52_2F1R, A52_DOLBY):
	mix21toS_SSE (samples, bias);
	break;

    case CONVERT (A52_3F1R, A52_STEREO):
	if (slev == 0)
	    goto mix_3to2_SSE;
	mix31to2_SSE (samples, bias);
	break;

    case CONVERT (A52_3F1R, A52_DOLBY):
	mix31toS_SSE (samples, bias);
	break;

    case CONVERT (A52_2F2R, A52_STEREO):
	if (slev == 0)
	    break;
	mix2to1_SSE (samples, samples + 512, bias);
	mix2to1_SSE (samples + 256, samples + 768, bias);
	break;

    case CONVERT (A52_2F2R, A52_DOLBY):
	mix22toS_SSE (samples, bias);
	break;

    case CONVERT (A52_3F2R, A52_STEREO):
	if (slev == 0)
	    goto mix_3to2_SSE;
	mix32to2_SSE (samples, bias);
	break;

    case CONVERT (A52_3F2R, A52_DOLBY):
	mix32toS_SSE (samples, bias);
	break;

    case CONVERT (A52_3F1R, A52_3F):
	if (slev == 0)
	    break;
	mix21to2_SSE (samples, samples + 512, bias);
	break;

    case CONVERT (A52_3F2R, A52_3F):
	if (slev == 0)
	    break;
	mix2to1_SSE (samples, samples + 768, bias);
	mix2to1_SSE (samples + 512, samples + 1024, bias);
	break;

    case CONVERT (A52_3F1R, A52_2F1R):
	mix3to2_SSE (samples, bias);
	memcpy (samples + 512, samples + 768, 256 * sizeof (sample_t));
	break;

    case CONVERT (A52_2F2R, A52_2F1R):
	mix2to1_SSE (samples + 512, samples + 768, bias);
	break;

    case CONVERT (A52_3F2R, A52_2F1R):
	mix3to2_SSE (samples, bias); //FIXME possible bug? (output doesnt seem to be used)
	move2to1_SSE (samples + 768, samples + 512, bias);
	break;

    case CONVERT (A52_3F2R, A52_3F1R):
	mix2to1_SSE (samples + 768, samples + 1024, bias);
	break;

    case CONVERT (A52_2F1R, A52_2F2R):
	memcpy (samples + 768, samples + 512, 256 * sizeof (sample_t));
	break;

    case CONVERT (A52_3F1R, A52_2F2R):
	mix3to2_SSE (samples, bias);
	memcpy (samples + 512, samples + 768, 256 * sizeof (sample_t));
	break;

    case CONVERT (A52_3F2R, A52_2F2R):
	mix3to2_SSE (samples, bias);
	memcpy (samples + 512, samples + 768, 256 * sizeof (sample_t));
	memcpy (samples + 768, samples + 1024, 256 * sizeof (sample_t));
	break;

    case CONVERT (A52_3F1R, A52_3F2R):
	memcpy (samples + 1024, samples + 768, 256 * sizeof (sample_t));
	break;
    }
}

static void upmix_MMX (sample_t * samples, int acmod, int output)
{
    switch (CONVERT (acmod, output & A52_CHANNEL_MASK)) {

    case CONVERT (A52_CHANNEL, A52_CHANNEL2):
	memcpy (samples + 256, samples, 256 * sizeof (sample_t));
	break;

    case CONVERT (A52_3F2R, A52_MONO):
	zero_MMX (samples + 1024);
    case CONVERT (A52_3F1R, A52_MONO):
    case CONVERT (A52_2F2R, A52_MONO):
	zero_MMX (samples + 768);
    case CONVERT (A52_3F, A52_MONO):
    case CONVERT (A52_2F1R, A52_MONO):
	zero_MMX (samples + 512);
    case CONVERT (A52_CHANNEL, A52_MONO):
    case CONVERT (A52_STEREO, A52_MONO):
	zero_MMX (samples + 256);
	break;

    case CONVERT (A52_3F2R, A52_STEREO):
    case CONVERT (A52_3F2R, A52_DOLBY):
	zero_MMX (samples + 1024);
    case CONVERT (A52_3F1R, A52_STEREO):
    case CONVERT (A52_3F1R, A52_DOLBY):
	zero_MMX (samples + 768);
    case CONVERT (A52_3F, A52_STEREO):
    case CONVERT (A52_3F, A52_DOLBY):
    mix_3to2_MMX:
	memcpy (samples + 512, samples + 256, 256 * sizeof (sample_t));
	zero_MMX (samples + 256);
	break;

    case CONVERT (A52_2F2R, A52_STEREO):
    case CONVERT (A52_2F2R, A52_DOLBY):
	zero_MMX (samples + 768);
    case CONVERT (A52_2F1R, A52_STEREO):
    case CONVERT (A52_2F1R, A52_DOLBY):
	zero_MMX (samples + 512);
	break;

    case CONVERT (A52_3F2R, A52_3F):
	zero_MMX (samples + 1024);
    case CONVERT (A52_3F1R, A52_3F):
    case CONVERT (A52_2F2R, A52_2F1R):
	zero_MMX (samples + 768);
	break;

    case CONVERT (A52_3F2R, A52_3F1R):
	zero_MMX (samples + 1024);
	break;

    case CONVERT (A52_3F2R, A52_2F1R):
	zero_MMX (samples + 1024);
    case CONVERT (A52_3F1R, A52_2F1R):
    mix_31to21_MMX:
	memcpy (samples + 768, samples + 512, 256 * sizeof (sample_t));
	goto mix_3to2_MMX;

    case CONVERT (A52_3F2R, A52_2F2R):
	memcpy (samples + 1024, samples + 768, 256 * sizeof (sample_t));
	goto mix_31to21_MMX;
    }
}

static void mix2to1_3dnow (sample_t * dest, sample_t * src, sample_t bias)
{
	__asm__ volatile(
	"movd  %2, %%mm7	\n\t"
	"punpckldq %2, %%mm7	\n\t"
	"mov $-1024, %%"REG_S"	\n\t"
	ASMALIGN(4)
	"1:			\n\t"
	"movq  (%0, %%"REG_S"), %%mm0	\n\t" 
	"movq  8(%0, %%"REG_S"), %%mm1	\n\t"
	"movq  16(%0, %%"REG_S"), %%mm2	\n\t" 
	"movq  24(%0, %%"REG_S"), %%mm3	\n\t"
	"pfadd (%1, %%"REG_S"), %%mm0	\n\t" 
	"pfadd 8(%1, %%"REG_S"), %%mm1	\n\t"
	"pfadd 16(%1, %%"REG_S"), %%mm2	\n\t" 
	"pfadd 24(%1, %%"REG_S"), %%mm3	\n\t"
	"pfadd %%mm7, %%mm0		\n\t"
	"pfadd %%mm7, %%mm1		\n\t"
	"pfadd %%mm7, %%mm2		\n\t"
	"pfadd %%mm7, %%mm3		\n\t"
	"movq  %%mm0, (%1, %%"REG_S")	\n\t"
	"movq  %%mm1, 8(%1, %%"REG_S")	\n\t"
	"movq  %%mm2, 16(%1, %%"REG_S")	\n\t"
	"movq  %%mm3, 24(%1, %%"REG_S")	\n\t"
	"add $32, %%"REG_S"		\n\t"
	" jnz 1b			\n\t"
	:: "r" (src+256), "r" (dest+256), "m" (bias)
	: "%"REG_S
	);
}

static void mix3to1_3dnow (sample_t * samples, sample_t bias)
{
	__asm__ volatile(
	"movd  %1, %%mm7	\n\t"
	"punpckldq %1, %%mm7	\n\t"
	"mov $-1024, %%"REG_S"	\n\t"
	ASMALIGN(4)
	"1:			\n\t"
	"movq  (%0, %%"REG_S"), %%mm0	\n\t" 
	"movq  8(%0, %%"REG_S"), %%mm1	\n\t"
	"movq  1024(%0, %%"REG_S"), %%mm2\n\t" 
	"movq  1032(%0, %%"REG_S"), %%mm3\n\t"
	"pfadd 2048(%0, %%"REG_S"), %%mm0\n\t" 
	"pfadd 2056(%0, %%"REG_S"), %%mm1\n\t"
	"pfadd %%mm7, %%mm0		\n\t"
	"pfadd %%mm7, %%mm1		\n\t"
	"pfadd %%mm2, %%mm0		\n\t"
	"pfadd %%mm3, %%mm1		\n\t"
	"movq  %%mm0, (%0, %%"REG_S")	\n\t"
	"movq  %%mm1, 8(%0, %%"REG_S")	\n\t"
	"add $16, %%"REG_S"		\n\t"
	" jnz 1b			\n\t"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S
	);
}

static void mix4to1_3dnow (sample_t * samples, sample_t bias)
{
	__asm__ volatile(
	"movd  %1, %%mm7	\n\t"
	"punpckldq %1, %%mm7	\n\t"
	"mov $-1024, %%"REG_S"	\n\t"
	ASMALIGN(4)
	"1:			\n\t"
	"movq  (%0, %%"REG_S"), %%mm0	\n\t" 
	"movq  8(%0, %%"REG_S"), %%mm1	\n\t"
	"movq  1024(%0, %%"REG_S"), %%mm2\n\t" 
	"movq  1032(%0, %%"REG_S"), %%mm3\n\t"
	"pfadd 2048(%0, %%"REG_S"), %%mm0\n\t" 
	"pfadd 2056(%0, %%"REG_S"), %%mm1\n\t"
	"pfadd 3072(%0, %%"REG_S"), %%mm2\n\t" 
	"pfadd 3080(%0, %%"REG_S"), %%mm3\n\t"
	"pfadd %%mm7, %%mm0		\n\t"
	"pfadd %%mm7, %%mm1		\n\t"
	"pfadd %%mm2, %%mm0		\n\t"
	"pfadd %%mm3, %%mm1		\n\t"
	"movq  %%mm0, (%0, %%"REG_S")	\n\t"
	"movq  %%mm1, 8(%0, %%"REG_S")	\n\t"
	"add $16, %%"REG_S"		\n\t"
	" jnz 1b			\n\t"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S
	);
}

static void mix5to1_3dnow (sample_t * samples, sample_t bias)
{
	__asm__ volatile(
	"movd  %1, %%mm7	\n\t"
	"punpckldq %1, %%mm7	\n\t"
	"mov $-1024, %%"REG_S"	\n\t"
	ASMALIGN(4)
	"1:			\n\t"
	"movq  (%0, %%"REG_S"), %%mm0	\n\t" 
	"movq  8(%0, %%"REG_S"), %%mm1	\n\t"
	"movq  1024(%0, %%"REG_S"), %%mm2\n\t" 
	"movq  1032(%0, %%"REG_S"), %%mm3\n\t"
	"pfadd 2048(%0, %%"REG_S"), %%mm0\n\t" 
	"pfadd 2056(%0, %%"REG_S"), %%mm1\n\t"
	"pfadd 3072(%0, %%"REG_S"), %%mm2\n\t" 
	"pfadd 3080(%0, %%"REG_S"), %%mm3\n\t"
	"pfadd %%mm7, %%mm0		\n\t"
	"pfadd %%mm7, %%mm1		\n\t"
	"pfadd 4096(%0, %%"REG_S"), %%mm2\n\t" 
	"pfadd 4104(%0, %%"REG_S"), %%mm3\n\t"
	"pfadd %%mm2, %%mm0		\n\t"
	"pfadd %%mm3, %%mm1		\n\t"
	"movq  %%mm0, (%0, %%"REG_S")	\n\t"
	"movq  %%mm1, 8(%0, %%"REG_S")	\n\t"
	"add $16, %%"REG_S"		\n\t"
	" jnz 1b			\n\t"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S
	);
}

static void mix3to2_3dnow (sample_t * samples, sample_t bias)
{
	__asm__ volatile(
	"movd  %1, %%mm7	\n\t"
	"punpckldq %1, %%mm7	\n\t"
	"mov $-1024, %%"REG_S"	\n\t"
	ASMALIGN(4)
	"1:			\n\t"
	"movq   1024(%0, %%"REG_S"), %%mm0\n\t" 
	"movq   1032(%0, %%"REG_S"), %%mm1\n\t"
	"pfadd  %%mm7, %%mm0		\n\t" //common
	"pfadd  %%mm7, %%mm1		\n\t" //common
	"movq   (%0, %%"REG_S"), %%mm2	\n\t" 
	"movq   8(%0, %%"REG_S"), %%mm3	\n\t"
	"movq   2048(%0, %%"REG_S"), %%mm4\n\t"
	"movq   2056(%0, %%"REG_S"), %%mm5\n\t"
	"pfadd  %%mm0, %%mm2		\n\t"
	"pfadd  %%mm1, %%mm3		\n\t"
	"pfadd  %%mm0, %%mm4		\n\t"
	"pfadd  %%mm1, %%mm5		\n\t"
	"movq   %%mm2, (%0, %%"REG_S")	\n\t"
	"movq   %%mm3, 8(%0, %%"REG_S")	\n\t"
	"movq   %%mm4, 1024(%0, %%"REG_S")\n\t"
	"movq   %%mm5, 1032(%0, %%"REG_S")\n\t"
	"add $16, %%"REG_S"		\n\t"
	" jnz 1b			\n\t"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S
	);
}

static void mix21to2_3dnow (sample_t * left, sample_t * right, sample_t bias)
{
	__asm__ volatile(
		"movd  %2, %%mm7	\n\t"
		"punpckldq %2, %%mm7	\n\t"
		"mov $-1024, %%"REG_S"	\n\t"
		ASMALIGN(4)
		"1:			\n\t"
		"movq  1024(%1, %%"REG_S"), %%mm0\n\t" 
		"movq  1032(%1, %%"REG_S"), %%mm1\n\t"
		"pfadd %%mm7, %%mm0		\n\t" //common
		"pfadd %%mm7, %%mm1		\n\t" //common
		"movq  (%0, %%"REG_S"), %%mm2	\n\t" 
		"movq  8(%0, %%"REG_S"), %%mm3	\n\t"
		"movq  (%1, %%"REG_S"), %%mm4	\n\t"
		"movq  8(%1, %%"REG_S"), %%mm5	\n\t"
		"pfadd %%mm0, %%mm2		\n\t"
		"pfadd %%mm1, %%mm3		\n\t"
		"pfadd %%mm0, %%mm4		\n\t"
		"pfadd %%mm1, %%mm5		\n\t"
		"movq  %%mm2, (%0, %%"REG_S")	\n\t"
		"movq  %%mm3, 8(%0, %%"REG_S")	\n\t"
		"movq  %%mm4, (%1, %%"REG_S")	\n\t"
		"movq  %%mm5, 8(%1, %%"REG_S")	\n\t"
		"add $16, %%"REG_S"		\n\t"
		" jnz 1b			\n\t"
	:: "r" (left+256), "r" (right+256), "m" (bias)
	: "%"REG_S
	);
}

static void mix21toS_3dnow (sample_t * samples, sample_t bias)
{
	__asm__ volatile(
		"movd  %1, %%mm7	\n\t"
		"punpckldq %1, %%mm7	\n\t"
		"mov $-1024, %%"REG_S"	\n\t"
		ASMALIGN(4)
		"1:			\n\t"
		"movq  2048(%0, %%"REG_S"), %%mm0\n\t"  // surround
		"movq  2056(%0, %%"REG_S"), %%mm1\n\t"  // surround
		"movq  (%0, %%"REG_S"), %%mm2	\n\t" 
		"movq  8(%0, %%"REG_S"), %%mm3	\n\t"
		"movq  1024(%0, %%"REG_S"), %%mm4\n\t"
		"movq  1032(%0, %%"REG_S"), %%mm5\n\t"
		"pfadd %%mm7, %%mm2		\n\t"
		"pfadd %%mm7, %%mm3		\n\t"
		"pfadd %%mm7, %%mm4		\n\t"
		"pfadd %%mm7, %%mm5		\n\t"
		"pfsub %%mm0, %%mm2		\n\t"
		"pfsub %%mm1, %%mm3		\n\t"
		"pfadd %%mm0, %%mm4		\n\t"
		"pfadd %%mm1, %%mm5		\n\t"
		"movq  %%mm2, (%0, %%"REG_S")	\n\t"
		"movq  %%mm3, 8(%0, %%"REG_S")	\n\t"
		"movq  %%mm4, 1024(%0, %%"REG_S")\n\t"
		"movq  %%mm5, 1032(%0, %%"REG_S")\n\t"
		"add $16, %%"REG_S"		\n\t"
		" jnz 1b			\n\t"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S
	);
}

static void mix31to2_3dnow (sample_t * samples, sample_t bias)
{
	__asm__ volatile(
		"movd  %1, %%mm7	\n\t"
		"punpckldq %1, %%mm7	\n\t"
		"mov $-1024, %%"REG_S"	\n\t"
		ASMALIGN(4)
		"1:			\n\t"
		"movq  1024(%0, %%"REG_S"), %%mm0\n\t"  
		"movq  1032(%0, %%"REG_S"), %%mm1\n\t"
		"pfadd 3072(%0, %%"REG_S"), %%mm0\n\t"  
		"pfadd 3080(%0, %%"REG_S"), %%mm1\n\t"
		"pfadd %%mm7, %%mm0		\n\t" // common
		"pfadd %%mm7, %%mm1		\n\t" // common
		"movq  (%0, %%"REG_S"), %%mm2	\n\t" 
		"movq  8(%0, %%"REG_S"), %%mm3	\n\t"
		"movq  2048(%0, %%"REG_S"), %%mm4\n\t"
		"movq  2056(%0, %%"REG_S"), %%mm5\n\t"
		"pfadd %%mm0, %%mm2		\n\t"
		"pfadd %%mm1, %%mm3		\n\t"
		"pfadd %%mm0, %%mm4		\n\t"
		"pfadd %%mm1, %%mm5		\n\t"
		"movq  %%mm2, (%0, %%"REG_S")	\n\t"
		"movq  %%mm3, 8(%0, %%"REG_S")	\n\t"
		"movq  %%mm4, 1024(%0, %%"REG_S")\n\t"
		"movq  %%mm5, 1032(%0, %%"REG_S")\n\t"
		"add $16, %%"REG_S"		\n\t"
		" jnz 1b			\n\t"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S
	);
}

static void mix31toS_3dnow (sample_t * samples, sample_t bias)
{
	__asm__ volatile(
		"movd  %1, %%mm7	\n\t"
		"punpckldq %1, %%mm7	\n\t"
		"mov $-1024, %%"REG_S"	\n\t"
		ASMALIGN(4)
		"1:			\n\t"
		"movq   1024(%0, %%"REG_S"), %%mm0\n\t"  
		"movq   1032(%0, %%"REG_S"), %%mm1\n\t"
		"pfadd  %%mm7, %%mm0		\n\t" // common
		"pfadd  %%mm7, %%mm1		\n\t" // common
		"movq   (%0, %%"REG_S"), %%mm2	\n\t" 
		"movq   8(%0, %%"REG_S"), %%mm3	\n\t"
		"movq   2048(%0, %%"REG_S"), %%mm4\n\t"
		"movq   2056(%0, %%"REG_S"), %%mm5\n\t"
		"pfadd  %%mm0, %%mm2		\n\t"
		"pfadd  %%mm1, %%mm3		\n\t"
		"pfadd  %%mm0, %%mm4		\n\t"
		"pfadd  %%mm1, %%mm5		\n\t"
		"movq   3072(%0, %%"REG_S"), %%mm0\n\t" // surround
		"movq   3080(%0, %%"REG_S"), %%mm1\n\t" // surround
		"pfsub  %%mm0, %%mm2		\n\t"
		"pfsub  %%mm1, %%mm3		\n\t"
		"pfadd  %%mm0, %%mm4		\n\t"
		"pfadd  %%mm1, %%mm5		\n\t"
		"movq   %%mm2, (%0, %%"REG_S")	\n\t"
		"movq   %%mm3, 8(%0, %%"REG_S")	\n\t"
		"movq   %%mm4, 1024(%0, %%"REG_S")\n\t"
		"movq   %%mm5, 1032(%0, %%"REG_S")\n\t"
		"add $16, %%"REG_S"		\n\t"
		" jnz 1b			\n\t"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S
	);
}

static void mix22toS_3dnow (sample_t * samples, sample_t bias)
{
	__asm__ volatile(
		"movd  %1, %%mm7	\n\t"
		"punpckldq %1, %%mm7	\n\t"
		"mov $-1024, %%"REG_S"	\n\t"
		ASMALIGN(4)
		"1:			\n\t"
		"movq  2048(%0, %%"REG_S"), %%mm0\n\t"  
		"movq  2056(%0, %%"REG_S"), %%mm1\n\t"
		"pfadd 3072(%0, %%"REG_S"), %%mm0\n\t" // surround
		"pfadd 3080(%0, %%"REG_S"), %%mm1\n\t" // surround
		"movq  (%0, %%"REG_S"), %%mm2	\n\t" 
		"movq  8(%0, %%"REG_S"), %%mm3	\n\t"
		"movq  1024(%0, %%"REG_S"), %%mm4\n\t"
		"movq  1032(%0, %%"REG_S"), %%mm5\n\t"
		"pfadd %%mm7, %%mm2		\n\t"
		"pfadd %%mm7, %%mm3		\n\t"
		"pfadd %%mm7, %%mm4		\n\t"
		"pfadd %%mm7, %%mm5		\n\t"
		"pfsub %%mm0, %%mm2		\n\t"
		"pfsub %%mm1, %%mm3		\n\t"
		"pfadd %%mm0, %%mm4		\n\t"
		"pfadd %%mm1, %%mm5		\n\t"
		"movq  %%mm2, (%0, %%"REG_S")	\n\t"
		"movq  %%mm3, 8(%0, %%"REG_S")	\n\t"
		"movq  %%mm4, 1024(%0, %%"REG_S")\n\t"
		"movq  %%mm5, 1032(%0, %%"REG_S")\n\t"
		"add $16, %%"REG_S"		\n\t"
		" jnz 1b			\n\t"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S
	);
}

static void mix32to2_3dnow (sample_t * samples, sample_t bias)
{
	__asm__ volatile(
	"movd  %1, %%mm7	\n\t"
	"punpckldq %1, %%mm7	\n\t"
	"mov $-1024, %%"REG_S"	\n\t"
	ASMALIGN(4)
	"1:			\n\t"
	"movq   1024(%0, %%"REG_S"), %%mm0\n\t" 
	"movq   1032(%0, %%"REG_S"), %%mm1\n\t"
	"pfadd  %%mm7, %%mm0		\n\t" // common
	"pfadd  %%mm7, %%mm1		\n\t" // common
	"movq   %%mm0, %%mm2		\n\t" // common
	"movq   %%mm1, %%mm3		\n\t" // common
	"pfadd  (%0, %%"REG_S"), %%mm0	\n\t" 
	"pfadd  8(%0, %%"REG_S"), %%mm1	\n\t"
	"pfadd  2048(%0, %%"REG_S"), %%mm2\n\t" 
	"pfadd  2056(%0, %%"REG_S"), %%mm3\n\t"
	"pfadd  3072(%0, %%"REG_S"), %%mm0\n\t" 
	"pfadd  3080(%0, %%"REG_S"), %%mm1\n\t"
	"pfadd  4096(%0, %%"REG_S"), %%mm2\n\t" 
	"pfadd  4104(%0, %%"REG_S"), %%mm3\n\t"
	"movq   %%mm0, (%0, %%"REG_S")	\n\t"
	"movq   %%mm1, 8(%0, %%"REG_S")	\n\t"
	"movq   %%mm2, 1024(%0, %%"REG_S")\n\t"
	"movq   %%mm3, 1032(%0, %%"REG_S")\n\t"
	"add $16, %%"REG_S"		\n\t"
	" jnz 1b			\n\t"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S
	);
}

/* todo: should be optimized better */
static void mix32toS_3dnow (sample_t * samples, sample_t bias)
{
	__asm__ volatile(
	"mov $-1024, %%"REG_S"		\n\t"
	ASMALIGN(4)
	"1:			\n\t"
	"movd  %1, %%mm7		\n\t"
	"punpckldq %1, %%mm7		\n\t"
	"movq  1024(%0, %%"REG_S"), %%mm0\n\t" 
	"movq  1032(%0, %%"REG_S"), %%mm1\n\t"
	"movq  3072(%0, %%"REG_S"), %%mm4\n\t" 
	"movq  3080(%0, %%"REG_S"), %%mm5\n\t"
	"pfadd %%mm7, %%mm0		\n\t" // common
	"pfadd %%mm7, %%mm1		\n\t" // common
	"pfadd 4096(%0, %%"REG_S"), %%mm4\n\t" // surround	
	"pfadd 4104(%0, %%"REG_S"), %%mm5\n\t" // surround
	"movq  (%0, %%"REG_S"), %%mm2	\n\t" 
	"movq  8(%0, %%"REG_S"), %%mm3	\n\t"
	"movq  2048(%0, %%"REG_S"), %%mm6\n\t" 
	"movq  2056(%0, %%"REG_S"), %%mm7\n\t"
	"pfsub %%mm4, %%mm2		\n\t"	
	"pfsub %%mm5, %%mm3		\n\t"
	"pfadd %%mm4, %%mm6		\n\t"	
	"pfadd %%mm5, %%mm7		\n\t"
	"pfadd %%mm0, %%mm2		\n\t"	
	"pfadd %%mm1, %%mm3		\n\t"
	"pfadd %%mm0, %%mm6		\n\t"	
	"pfadd %%mm1, %%mm7		\n\t"
	"movq  %%mm2, (%0, %%"REG_S")	\n\t"
	"movq  %%mm3, 8(%0, %%"REG_S")	\n\t"
	"movq  %%mm6, 1024(%0, %%"REG_S")\n\t"
	"movq  %%mm7, 1032(%0, %%"REG_S")\n\t"
	"add $16, %%"REG_S"		\n\t"
	" jnz 1b			\n\t"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S
	);
}

static void move2to1_3dnow (sample_t * src, sample_t * dest, sample_t bias)
{
	__asm__ volatile(
		"movd  %2, %%mm7	\n\t"
		"punpckldq %2, %%mm7	\n\t"
		"mov $-1024, %%"REG_S"	\n\t"
		ASMALIGN(4)
		"1:			\n\t"
		"movq  (%0, %%"REG_S"), %%mm0	\n\t"  
		"movq  8(%0, %%"REG_S"), %%mm1	\n\t"
		"movq  16(%0, %%"REG_S"), %%mm2	\n\t"  
		"movq  24(%0, %%"REG_S"), %%mm3	\n\t"
		"pfadd 1024(%0, %%"REG_S"), %%mm0\n\t"
		"pfadd 1032(%0, %%"REG_S"), %%mm1\n\t"
		"pfadd 1040(%0, %%"REG_S"), %%mm2\n\t"
		"pfadd 1048(%0, %%"REG_S"), %%mm3\n\t"
		"pfadd %%mm7, %%mm0		\n\t"
		"pfadd %%mm7, %%mm1		\n\t"
		"pfadd %%mm7, %%mm2		\n\t"
		"pfadd %%mm7, %%mm3		\n\t"
		"movq  %%mm0, (%1, %%"REG_S")	\n\t"
		"movq  %%mm1, 8(%1, %%"REG_S")	\n\t"
		"movq  %%mm2, 16(%1, %%"REG_S")	\n\t"
		"movq  %%mm3, 24(%1, %%"REG_S")	\n\t"
		"add $32, %%"REG_S"		\n\t"
		" jnz 1b			\n\t"
	:: "r" (src+256), "r" (dest+256), "m" (bias)
	: "%"REG_S
	);
}

static void downmix_3dnow (sample_t * samples, int acmod, int output, sample_t bias,
	      sample_t clev, sample_t slev)
{
    switch (CONVERT (acmod, output & A52_CHANNEL_MASK)) {

    case CONVERT (A52_CHANNEL, A52_CHANNEL2):
	memcpy (samples, samples + 256, 256 * sizeof (sample_t));
	break;

    case CONVERT (A52_CHANNEL, A52_MONO):
    case CONVERT (A52_STEREO, A52_MONO):
    mix_2to1_3dnow:
	mix2to1_3dnow (samples, samples + 256, bias);
	break;

    case CONVERT (A52_2F1R, A52_MONO):
	if (slev == 0)
	    goto mix_2to1_3dnow;
    case CONVERT (A52_3F, A52_MONO):
    mix_3to1_3dnow:
	mix3to1_3dnow (samples, bias);
	break;

    case CONVERT (A52_3F1R, A52_MONO):
	if (slev == 0)
	    goto mix_3to1_3dnow;
    case CONVERT (A52_2F2R, A52_MONO):
	if (slev == 0)
	    goto mix_2to1_3dnow;
	mix4to1_3dnow (samples, bias);
	break;

    case CONVERT (A52_3F2R, A52_MONO):
	if (slev == 0)
	    goto mix_3to1_3dnow;
	mix5to1_3dnow (samples, bias);
	break;

    case CONVERT (A52_MONO, A52_DOLBY):
	memcpy (samples + 256, samples, 256 * sizeof (sample_t));
	break;

    case CONVERT (A52_3F, A52_STEREO):
    case CONVERT (A52_3F, A52_DOLBY):
    mix_3to2_3dnow:
	mix3to2_3dnow (samples, bias);
	break;

    case CONVERT (A52_2F1R, A52_STEREO):
	if (slev == 0)
	    break;
	mix21to2_3dnow (samples, samples + 256, bias);
	break;

    case CONVERT (A52_2F1R, A52_DOLBY):
	mix21toS_3dnow (samples, bias);
	break;

    case CONVERT (A52_3F1R, A52_STEREO):
	if (slev == 0)
	    goto mix_3to2_3dnow;
	mix31to2_3dnow (samples, bias);
	break;

    case CONVERT (A52_3F1R, A52_DOLBY):
	mix31toS_3dnow (samples, bias);
	break;

    case CONVERT (A52_2F2R, A52_STEREO):
	if (slev == 0)
	    break;
	mix2to1_3dnow (samples, samples + 512, bias);
	mix2to1_3dnow (samples + 256, samples + 768, bias);
	break;

    case CONVERT (A52_2F2R, A52_DOLBY):
	mix22toS_3dnow (samples, bias);
	break;

    case CONVERT (A52_3F2R, A52_STEREO):
	if (slev == 0)
	    goto mix_3to2_3dnow;
	mix32to2_3dnow (samples, bias);
	break;

    case CONVERT (A52_3F2R, A52_DOLBY):
	mix32toS_3dnow (samples, bias);
	break;

    case CONVERT (A52_3F1R, A52_3F):
	if (slev == 0)
	    break;
	mix21to2_3dnow (samples, samples + 512, bias);
	break;

    case CONVERT (A52_3F2R, A52_3F):
	if (slev == 0)
	    break;
	mix2to1_3dnow (samples, samples + 768, bias);
	mix2to1_3dnow (samples + 512, samples + 1024, bias);
	break;

    case CONVERT (A52_3F1R, A52_2F1R):
	mix3to2_3dnow (samples, bias);
	memcpy (samples + 512, samples + 768, 256 * sizeof (sample_t));
	break;

    case CONVERT (A52_2F2R, A52_2F1R):
	mix2to1_3dnow (samples + 512, samples + 768, bias);
	break;

    case CONVERT (A52_3F2R, A52_2F1R):
	mix3to2_3dnow (samples, bias); //FIXME possible bug? (output doesnt seem to be used)
	move2to1_3dnow (samples + 768, samples + 512, bias);
	break;

    case CONVERT (A52_3F2R, A52_3F1R):
	mix2to1_3dnow (samples + 768, samples + 1024, bias);
	break;

    case CONVERT (A52_2F1R, A52_2F2R):
	memcpy (samples + 768, samples + 512, 256 * sizeof (sample_t));
	break;

    case CONVERT (A52_3F1R, A52_2F2R):
	mix3to2_3dnow (samples, bias);
	memcpy (samples + 512, samples + 768, 256 * sizeof (sample_t));
	break;

    case CONVERT (A52_3F2R, A52_2F2R):
	mix3to2_3dnow (samples, bias);
	memcpy (samples + 512, samples + 768, 256 * sizeof (sample_t));
	memcpy (samples + 768, samples + 1024, 256 * sizeof (sample_t));
	break;

    case CONVERT (A52_3F1R, A52_3F2R):
	memcpy (samples + 1024, samples + 768, 256 * sizeof (sample_t));
	break;
    }
    __asm__ volatile("femms":::"memory");
}

#endif // ARCH_X86 || ARCH_X86_64
