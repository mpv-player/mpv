#ifndef _OS_H
#define _OS_H
/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis 'TREMOR' CODEC SOURCE CODE.   *
 *                                                                  *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis 'TREMOR' SOURCE CODE IS (C) COPYRIGHT 1994-2002    *
 * BY THE Xiph.Org FOUNDATION http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: #ifdef jail to whip a few platforms into the UNIX ideal.

 ********************************************************************/

#include <math.h>
#include "os_types.h"

#define STIN static inline

#ifndef M_PI
#  define M_PI (3.1415926536f)
#endif

#ifdef _WIN32
#  include <malloc.h>
#  define rint(x)   (floor((x)+0.5f)) 
#  define NO_FLOAT_MATH_LIB
#  define FAST_HYPOT(a, b) sqrt((a)*(a) + (b)*(b))
#endif

#ifdef HAVE_ALLOCA_H
#  include <alloca.h>
#endif

#ifdef USE_MEMORY_H
#  include <memory.h>
#endif

#ifndef min
#  define min(x,y)  ((x)>(y)?(y):(x))
#endif

#ifndef max
#  define max(x,y)  ((x)<(y)?(y):(x))
#endif

#endif /* _OS_H */
