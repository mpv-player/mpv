// this code is based on a52dec/libao/audio_out_oss.c
// AltiVec support Copyright (c) 2004 Romain Dolbeau <romain@dolbeau.org>

#ifndef SYS_DARWIN
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
    
    vec_st(r0, 0, s16);
    vec_st(r1, 16, s16);

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

