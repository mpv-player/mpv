/*
 * resample.c
 * Copyright (C) 2004 Romain Dolbeau <romain@dolbeau.org>
 *
 * This file is part of a52dec, a free ATSC A-52 stream decoder.
 * See http://liba52.sourceforge.net/ for updates.
 *
 * File added for use with MPlayer and not part of original a52dec.
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

#ifdef HAVE_ALTIVEC_H
#include <altivec.h>
#endif

const vector signed int magic = {0x43c00000,0x43c00000,0x43c00000,0x43c00000};

static inline vector signed short convert16_altivec(vector signed int v1, vector signed int v2)
{
  register vector signed short result;
  v1 = vec_subs(v1, magic);
  v2 = vec_subs(v2, magic);
  result = vec_packs(v1, v2);

  return result;
}

static void unaligned_store(vector signed short value, int off, int16_t *dst)
{
    register vector unsigned char align = vec_lvsr(0, dst),
                                  mask = vec_lvsl(0, dst);
    register vector signed short t0,t1, edges;

    t0 = vec_ld(0+off, dst);
    t1 = vec_ld(15+off, dst);
    edges = vec_perm(t1 ,t0, mask);
    t1 = vec_perm(value, edges, align);
    t0 = vec_perm(edges, value, align);
    vec_st(t1, 15+off, dst);
    vec_st(t0, 0+off, dst);
}

static int a52_resample_STEREO_to_2_altivec(float * _f, int16_t * s16){
#if 0
  int i;
  int32_t * f = (int32_t *) _f;
  for (i = 0; i < 256; i++) {
    s16[2*i] = convert (f[i]);
    s16[2*i+1] = convert (f[i+256]);
  }
  return 2*256;
#else
  int i = 0;
  int32_t * f = (int32_t *) _f;
  register vector signed int f0, f4, f256, f260;
  register vector signed short reven, rodd, r0, r1;

  for (i = 0; i < 256; i+= 8) {
    f0 = vec_ld(0, f);
    f4 = vec_ld(16, f);

    f256 = vec_ld(1024, f);
    f260 = vec_ld(1040, f);

    reven = convert16_altivec(f0, f4);
    rodd = convert16_altivec(f256, f260);

    r0 = vec_mergeh(reven, rodd);
    r1 = vec_mergel(reven, rodd);
    // FIXME can be merged to spare some I/O
    unaligned_store(r0, 0, s16);
    unaligned_store(r1, 16, s16);

    f += 8;
    s16 += 16;
  }
  return(2*256);
#endif
}

static void* a52_resample_altivec(int flags, int ch){
fprintf(stderr, "Checking for AltiVec resampler : 0x%08x, %d\n", flags, ch);

  switch (flags) {
  case A52_CHANNEL:
  case A52_STEREO:
  case A52_DOLBY:
    if(ch==2) return a52_resample_STEREO_to_2_altivec;
    break;

  default:
	fprintf(stderr, "Unsupported flags: 0x%08x (%d channels)\n", flags, ch);
	break;
  }
  return NULL;
}

