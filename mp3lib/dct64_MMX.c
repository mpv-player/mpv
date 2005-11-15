/*
* This code was taken from http://www.mpg123.org
* See ChangeLog of mpg123-0.59s-pre.1 for detail
* Applied to mplayer by Nick Kurshev <nickols_k@mail.ru>
*/
#include "mangle.h"
#define real float /* ugly - but only way */

void dct64_MMX(real *a,real *b,real *c)
{
    char tmp[256];
    __asm __volatile(
"	movl %2,%%eax\n\t"
/* Phase 1*/
"	flds     (%%eax)\n\t"
"	leal 128+%3,%%edx\n\t"
"	fadds 124(%%eax)\n\t"
"	movl %0,%%esi\n\t"
"	fstps    (%%edx)\n\t"
"	movl %1,%%edi\n\t"

"	flds    4(%%eax)\n\t"
"	movl $"MANGLE(costab_mmx)",%%ebx\n\t"
"	fadds 120(%%eax)\n\t"
"	orl %%ecx,%%ecx\n\t"
"	fstps   4(%%edx)\n\t"

"	flds     (%%eax)\n\t"
"	leal %3,%%ecx\n\t"
"	fsubs 124(%%eax)\n\t"
"	fmuls    (%%ebx)\n\t"
"	fstps 124(%%edx)\n\t"

"	flds    4(%%eax)\n\t"
"	fsubs 120(%%eax)\n\t"
"	fmuls   4(%%ebx)\n\t"
"	fstps 120(%%edx)\n\t"

"	flds    8(%%eax)\n\t"
"	fadds 116(%%eax)\n\t"
"	fstps   8(%%edx)\n\t"

"	flds   12(%%eax)\n\t"
"	fadds 112(%%eax)\n\t"
"	fstps  12(%%edx)\n\t"

"	flds    8(%%eax)\n\t"
"	fsubs 116(%%eax)\n\t"
"	fmuls   8(%%ebx)\n\t"
"	fstps 116(%%edx)\n\t"

"	flds   12(%%eax)\n\t"
"	fsubs 112(%%eax)\n\t"
"	fmuls  12(%%ebx)\n\t"
"	fstps 112(%%edx)\n\t"

"	flds   16(%%eax)\n\t"
"	fadds 108(%%eax)\n\t"
"	fstps  16(%%edx)\n\t"

"	flds   20(%%eax)\n\t"
"	fadds 104(%%eax)\n\t"
"	fstps  20(%%edx)\n\t"

"	flds   16(%%eax)\n\t"
"	fsubs 108(%%eax)\n\t"
"	fmuls  16(%%ebx)\n\t"
"	fstps 108(%%edx)\n\t"

"	flds   20(%%eax)\n\t"
"	fsubs 104(%%eax)\n\t"
"	fmuls  20(%%ebx)\n\t"
"	fstps 104(%%edx)\n\t"

"	flds   24(%%eax)\n\t"
"	fadds 100(%%eax)\n\t"
"	fstps  24(%%edx)\n\t"

"	flds   28(%%eax)\n\t"
"	fadds  96(%%eax)\n\t"
"	fstps  28(%%edx)\n\t"

"	flds   24(%%eax)\n\t"
"	fsubs 100(%%eax)\n\t"
"	fmuls  24(%%ebx)\n\t"
"	fstps 100(%%edx)\n\t"

"	flds   28(%%eax)\n\t"
"	fsubs  96(%%eax)\n\t"
"	fmuls  28(%%ebx)\n\t"
"	fstps  96(%%edx)\n\t"

"	flds   32(%%eax)\n\t"
"	fadds  92(%%eax)\n\t"
"	fstps  32(%%edx)\n\t"

"	flds   36(%%eax)\n\t"
"	fadds  88(%%eax)\n\t"
"	fstps  36(%%edx)\n\t"

"	flds   32(%%eax)\n\t"
"	fsubs  92(%%eax)\n\t"
"	fmuls  32(%%ebx)\n\t"
"	fstps  92(%%edx)\n\t"

"	flds   36(%%eax)\n\t"
"	fsubs  88(%%eax)\n\t"
"	fmuls  36(%%ebx)\n\t"
"	fstps  88(%%edx)\n\t"

"	flds   40(%%eax)\n\t"
"	fadds  84(%%eax)\n\t"
"	fstps  40(%%edx)\n\t"

"	flds   44(%%eax)\n\t"
"	fadds  80(%%eax)\n\t"
"	fstps  44(%%edx)\n\t"

"	flds   40(%%eax)\n\t"
"	fsubs  84(%%eax)\n\t"
"	fmuls  40(%%ebx)\n\t"
"	fstps  84(%%edx)\n\t"

"	flds   44(%%eax)\n\t"
"	fsubs  80(%%eax)\n\t"
"	fmuls  44(%%ebx)\n\t"
"	fstps  80(%%edx)\n\t"

"	flds   48(%%eax)\n\t"
"	fadds  76(%%eax)\n\t"
"	fstps  48(%%edx)\n\t"

"	flds   52(%%eax)\n\t"
"	fadds  72(%%eax)\n\t"
"	fstps  52(%%edx)\n\t"

"	flds   48(%%eax)\n\t"
"	fsubs  76(%%eax)\n\t"
"	fmuls  48(%%ebx)\n\t"
"	fstps  76(%%edx)\n\t"

"	flds   52(%%eax)\n\t"
"	fsubs  72(%%eax)\n\t"
"	fmuls  52(%%ebx)\n\t"
"	fstps  72(%%edx)\n\t"

"	flds   56(%%eax)\n\t"
"	fadds  68(%%eax)\n\t"
"	fstps  56(%%edx)\n\t"

"	flds   60(%%eax)\n\t"
"	fadds  64(%%eax)\n\t"
"	fstps  60(%%edx)\n\t"

"	flds   56(%%eax)\n\t"
"	fsubs  68(%%eax)\n\t"
"	fmuls  56(%%ebx)\n\t"
"	fstps  68(%%edx)\n\t"

"	flds   60(%%eax)\n\t"
"	fsubs  64(%%eax)\n\t"
"	fmuls  60(%%ebx)\n\t"
"	fstps  64(%%edx)\n\t"

/* Phase 2*/

"	flds     (%%edx)\n\t"
"	fadds  60(%%edx)\n\t"
"	fstps    (%%ecx)\n\t"

"	flds    4(%%edx)\n\t"
"	fadds  56(%%edx)\n\t"
"	fstps   4(%%ecx)\n\t"

"	flds     (%%edx)\n\t"
"	fsubs  60(%%edx)\n\t"
"	fmuls  64(%%ebx)\n\t"
"	fstps  60(%%ecx)\n\t"

"	flds    4(%%edx)\n\t"
"	fsubs  56(%%edx)\n\t"
"	fmuls  68(%%ebx)\n\t"
"	fstps  56(%%ecx)\n\t"

"	flds    8(%%edx)\n\t"
"	fadds  52(%%edx)\n\t"
"	fstps   8(%%ecx)\n\t"

"	flds   12(%%edx)\n\t"
"	fadds  48(%%edx)\n\t"
"	fstps  12(%%ecx)\n\t"

"	flds    8(%%edx)\n\t"
"	fsubs  52(%%edx)\n\t"
"	fmuls  72(%%ebx)\n\t"
"	fstps  52(%%ecx)\n\t"

"	flds   12(%%edx)\n\t"
"	fsubs  48(%%edx)\n\t"
"	fmuls  76(%%ebx)\n\t"
"	fstps  48(%%ecx)\n\t"

"	flds   16(%%edx)\n\t"
"	fadds  44(%%edx)\n\t"
"	fstps  16(%%ecx)\n\t"

"	flds   20(%%edx)\n\t"
"	fadds  40(%%edx)\n\t"
"	fstps  20(%%ecx)\n\t"

"	flds   16(%%edx)\n\t"
"	fsubs  44(%%edx)\n\t"
"	fmuls  80(%%ebx)\n\t"
"	fstps  44(%%ecx)\n\t"

"	flds   20(%%edx)\n\t"
"	fsubs  40(%%edx)\n\t"
"	fmuls  84(%%ebx)\n\t"
"	fstps  40(%%ecx)\n\t"

"	flds   24(%%edx)\n\t"
"	fadds  36(%%edx)\n\t"
"	fstps  24(%%ecx)\n\t"

"	flds   28(%%edx)\n\t"
"	fadds  32(%%edx)\n\t"
"	fstps  28(%%ecx)\n\t"

"	flds   24(%%edx)\n\t"
"	fsubs  36(%%edx)\n\t"
"	fmuls  88(%%ebx)\n\t"
"	fstps  36(%%ecx)\n\t"

"	flds   28(%%edx)\n\t"
"	fsubs  32(%%edx)\n\t"
"	fmuls  92(%%ebx)\n\t"
"	fstps  32(%%ecx)\n\t"

/* Phase 3*/

"	flds   64(%%edx)\n\t"
"	fadds 124(%%edx)\n\t"
"	fstps  64(%%ecx)\n\t"

"	flds   68(%%edx)\n\t"
"	fadds 120(%%edx)\n\t"
"	fstps  68(%%ecx)\n\t"

"	flds  124(%%edx)\n\t"
"	fsubs  64(%%edx)\n\t"
"	fmuls  64(%%ebx)\n\t"
"	fstps 124(%%ecx)\n\t"

"	flds  120(%%edx)\n\t"
"	fsubs  68(%%edx)\n\t"
"	fmuls  68(%%ebx)\n\t"
"	fstps 120(%%ecx)\n\t"

"	flds   72(%%edx)\n\t"
"	fadds 116(%%edx)\n\t"
"	fstps  72(%%ecx)\n\t"

"	flds   76(%%edx)\n\t"
"	fadds 112(%%edx)\n\t"
"	fstps  76(%%ecx)\n\t"

"	flds  116(%%edx)\n\t"
"	fsubs  72(%%edx)\n\t"
"	fmuls  72(%%ebx)\n\t"
"	fstps 116(%%ecx)\n\t"

"	flds  112(%%edx)\n\t"
"	fsubs  76(%%edx)\n\t"
"	fmuls  76(%%ebx)\n\t"
"	fstps 112(%%ecx)\n\t"

"	flds   80(%%edx)\n\t"
"	fadds 108(%%edx)\n\t"
"	fstps  80(%%ecx)\n\t"

"	flds   84(%%edx)\n\t"
"	fadds 104(%%edx)\n\t"
"	fstps  84(%%ecx)\n\t"

"	flds  108(%%edx)\n\t"
"	fsubs  80(%%edx)\n\t"
"	fmuls  80(%%ebx)\n\t"
"	fstps 108(%%ecx)\n\t"

"	flds  104(%%edx)\n\t"
"	fsubs  84(%%edx)\n\t"
"	fmuls  84(%%ebx)\n\t"
"	fstps 104(%%ecx)\n\t"

"	flds   88(%%edx)\n\t"
"	fadds 100(%%edx)\n\t"
"	fstps  88(%%ecx)\n\t"

"	flds   92(%%edx)\n\t"
"	fadds  96(%%edx)\n\t"
"	fstps  92(%%ecx)\n\t"

"	flds  100(%%edx)\n\t"
"	fsubs  88(%%edx)\n\t"
"	fmuls  88(%%ebx)\n\t"
"	fstps 100(%%ecx)\n\t"

"	flds   96(%%edx)\n\t"
"	fsubs  92(%%edx)\n\t"
"	fmuls  92(%%ebx)\n\t"
"	fstps  96(%%ecx)\n\t"

/* Phase 4*/

"	flds     (%%ecx)\n\t"
"	fadds  28(%%ecx)\n\t"
"	fstps    (%%edx)\n\t"

"	flds     (%%ecx)\n\t"
"	fsubs  28(%%ecx)\n\t"
"	fmuls  96(%%ebx)\n\t"
"	fstps  28(%%edx)\n\t"

"	flds    4(%%ecx)\n\t"
"	fadds  24(%%ecx)\n\t"
"	fstps   4(%%edx)\n\t"

"	flds    4(%%ecx)\n\t"
"	fsubs  24(%%ecx)\n\t"
"	fmuls 100(%%ebx)\n\t"
"	fstps  24(%%edx)\n\t"

"	flds    8(%%ecx)\n\t"
"	fadds  20(%%ecx)\n\t"
"	fstps   8(%%edx)\n\t"

"	flds    8(%%ecx)\n\t"
"	fsubs  20(%%ecx)\n\t"
"	fmuls 104(%%ebx)\n\t"
"	fstps  20(%%edx)\n\t"

"	flds   12(%%ecx)\n\t"
"	fadds  16(%%ecx)\n\t"
"	fstps  12(%%edx)\n\t"

"	flds   12(%%ecx)\n\t"
"	fsubs  16(%%ecx)\n\t"
"	fmuls 108(%%ebx)\n\t"
"	fstps  16(%%edx)\n\t"

"	flds   32(%%ecx)\n\t"
"	fadds  60(%%ecx)\n\t"
"	fstps  32(%%edx)\n\t"

"	flds   60(%%ecx)\n\t"
"	fsubs  32(%%ecx)\n\t"
"	fmuls  96(%%ebx)\n\t"
"	fstps  60(%%edx)\n\t"

"	flds   36(%%ecx)\n\t"
"	fadds  56(%%ecx)\n\t"
"	fstps  36(%%edx)\n\t"

"	flds   56(%%ecx)\n\t"
"	fsubs  36(%%ecx)\n\t"
"	fmuls 100(%%ebx)\n\t"
"	fstps  56(%%edx)\n\t"

"	flds   40(%%ecx)\n\t"
"	fadds  52(%%ecx)\n\t"
"	fstps  40(%%edx)\n\t"

"	flds   52(%%ecx)\n\t"
"	fsubs  40(%%ecx)\n\t"
"	fmuls 104(%%ebx)\n\t"
"	fstps  52(%%edx)\n\t"

"	flds   44(%%ecx)\n\t"
"	fadds  48(%%ecx)\n\t"
"	fstps  44(%%edx)\n\t"

"	flds   48(%%ecx)\n\t"
"	fsubs  44(%%ecx)\n\t"
"	fmuls 108(%%ebx)\n\t"
"	fstps  48(%%edx)\n\t"

"	flds   64(%%ecx)\n\t"
"	fadds  92(%%ecx)\n\t"
"	fstps  64(%%edx)\n\t"

"	flds   64(%%ecx)\n\t"
"	fsubs  92(%%ecx)\n\t"
"	fmuls  96(%%ebx)\n\t"
"	fstps  92(%%edx)\n\t"

"	flds   68(%%ecx)\n\t"
"	fadds  88(%%ecx)\n\t"
"	fstps  68(%%edx)\n\t"

"	flds   68(%%ecx)\n\t"
"	fsubs  88(%%ecx)\n\t"
"	fmuls 100(%%ebx)\n\t"
"	fstps  88(%%edx)\n\t"

"	flds   72(%%ecx)\n\t"
"	fadds  84(%%ecx)\n\t"
"	fstps  72(%%edx)\n\t"

"	flds   72(%%ecx)\n\t"
"	fsubs  84(%%ecx)\n\t"
"	fmuls 104(%%ebx)\n\t"
"	fstps  84(%%edx)\n\t"

"	flds   76(%%ecx)\n\t"
"	fadds  80(%%ecx)\n\t"
"	fstps  76(%%edx)\n\t"

"	flds   76(%%ecx)\n\t"
"	fsubs  80(%%ecx)\n\t"
"	fmuls 108(%%ebx)\n\t"
"	fstps  80(%%edx)\n\t"

"	flds   96(%%ecx)\n\t"
"	fadds 124(%%ecx)\n\t"
"	fstps  96(%%edx)\n\t"

"	flds  124(%%ecx)\n\t"
"	fsubs  96(%%ecx)\n\t"
"	fmuls  96(%%ebx)\n\t"
"	fstps 124(%%edx)\n\t"

"	flds  100(%%ecx)\n\t"
"	fadds 120(%%ecx)\n\t"
"	fstps 100(%%edx)\n\t"

"	flds  120(%%ecx)\n\t"
"	fsubs 100(%%ecx)\n\t"
"	fmuls 100(%%ebx)\n\t"
"	fstps 120(%%edx)\n\t"

"	flds  104(%%ecx)\n\t"
"	fadds 116(%%ecx)\n\t"
"	fstps 104(%%edx)\n\t"

"	flds  116(%%ecx)\n\t"
"	fsubs 104(%%ecx)\n\t"
"	fmuls 104(%%ebx)\n\t"
"	fstps 116(%%edx)\n\t"

"	flds  108(%%ecx)\n\t"
"	fadds 112(%%ecx)\n\t"
"	fstps 108(%%edx)\n\t"

"	flds  112(%%ecx)\n\t"
"	fsubs 108(%%ecx)\n\t"
"	fmuls 108(%%ebx)\n\t"
"	fstps 112(%%edx)\n\t"

"	flds     (%%edx)\n\t"
"	fadds  12(%%edx)\n\t"
"	fstps    (%%ecx)\n\t"

"	flds     (%%edx)\n\t"
"	fsubs  12(%%edx)\n\t"
"	fmuls 112(%%ebx)\n\t"
"	fstps  12(%%ecx)\n\t"

"	flds    4(%%edx)\n\t"
"	fadds   8(%%edx)\n\t"
"	fstps   4(%%ecx)\n\t"

"	flds    4(%%edx)\n\t"
"	fsubs   8(%%edx)\n\t"
"	fmuls 116(%%ebx)\n\t"
"	fstps   8(%%ecx)\n\t"

"	flds   16(%%edx)\n\t"
"	fadds  28(%%edx)\n\t"
"	fstps  16(%%ecx)\n\t"

"	flds   28(%%edx)\n\t"
"	fsubs  16(%%edx)\n\t"
"	fmuls 112(%%ebx)\n\t"
"	fstps  28(%%ecx)\n\t"

"	flds   20(%%edx)\n\t"
"	fadds  24(%%edx)\n\t"
"	fstps  20(%%ecx)\n\t"

"	flds   24(%%edx)\n\t"
"	fsubs  20(%%edx)\n\t"
"	fmuls 116(%%ebx)\n\t"
"	fstps  24(%%ecx)\n\t"

"	flds   32(%%edx)\n\t"
"	fadds  44(%%edx)\n\t"
"	fstps  32(%%ecx)\n\t"

"	flds   32(%%edx)\n\t"
"	fsubs  44(%%edx)\n\t"
"	fmuls 112(%%ebx)\n\t"
"	fstps  44(%%ecx)\n\t"

"	flds   36(%%edx)\n\t"
"	fadds  40(%%edx)\n\t"
"	fstps  36(%%ecx)\n\t"

"	flds   36(%%edx)\n\t"
"	fsubs  40(%%edx)\n\t"
"	fmuls 116(%%ebx)\n\t"
"	fstps  40(%%ecx)\n\t"

"	flds   48(%%edx)\n\t"
"	fadds  60(%%edx)\n\t"
"	fstps  48(%%ecx)\n\t"

"	flds   60(%%edx)\n\t"
"	fsubs  48(%%edx)\n\t"
"	fmuls 112(%%ebx)\n\t"
"	fstps  60(%%ecx)\n\t"

"	flds   52(%%edx)\n\t"
"	fadds  56(%%edx)\n\t"
"	fstps  52(%%ecx)\n\t"

"	flds   56(%%edx)\n\t"
"	fsubs  52(%%edx)\n\t"
"	fmuls 116(%%ebx)\n\t"
"	fstps  56(%%ecx)\n\t"

"	flds   64(%%edx)\n\t"
"	fadds  76(%%edx)\n\t"
"	fstps  64(%%ecx)\n\t"

"	flds   64(%%edx)\n\t"
"	fsubs  76(%%edx)\n\t"
"	fmuls 112(%%ebx)\n\t"
"	fstps  76(%%ecx)\n\t"

"	flds   68(%%edx)\n\t"
"	fadds  72(%%edx)\n\t"
"	fstps  68(%%ecx)\n\t"

"	flds   68(%%edx)\n\t"
"	fsubs  72(%%edx)\n\t"
"	fmuls 116(%%ebx)\n\t"
"	fstps  72(%%ecx)\n\t"

"	flds   80(%%edx)\n\t"
"	fadds  92(%%edx)\n\t"
"	fstps  80(%%ecx)\n\t"

"	flds   92(%%edx)\n\t"
"	fsubs  80(%%edx)\n\t"
"	fmuls 112(%%ebx)\n\t"
"	fstps  92(%%ecx)\n\t"

"	flds   84(%%edx)\n\t"
"	fadds  88(%%edx)\n\t"
"	fstps  84(%%ecx)\n\t"

"	flds   88(%%edx)\n\t"
"	fsubs  84(%%edx)\n\t"
"	fmuls 116(%%ebx)\n\t"
"	fstps  88(%%ecx)\n\t"

"	flds   96(%%edx)\n\t"
"	fadds 108(%%edx)\n\t"
"	fstps  96(%%ecx)\n\t"

"	flds   96(%%edx)\n\t"
"	fsubs 108(%%edx)\n\t"
"	fmuls 112(%%ebx)\n\t"
"	fstps 108(%%ecx)\n\t"

"	flds  100(%%edx)\n\t"
"	fadds 104(%%edx)\n\t"
"	fstps 100(%%ecx)\n\t"

"	flds  100(%%edx)\n\t"
"	fsubs 104(%%edx)\n\t"
"	fmuls 116(%%ebx)\n\t"
"	fstps 104(%%ecx)\n\t"

"	flds  112(%%edx)\n\t"
"	fadds 124(%%edx)\n\t"
"	fstps 112(%%ecx)\n\t"

"	flds  124(%%edx)\n\t"
"	fsubs 112(%%edx)\n\t"
"	fmuls 112(%%ebx)\n\t"
"	fstps 124(%%ecx)\n\t"

"	flds  116(%%edx)\n\t"
"	fadds 120(%%edx)\n\t"
"	fstps 116(%%ecx)\n\t"

"	flds  120(%%edx)\n\t"
"	fsubs 116(%%edx)\n\t"
"	fmuls 116(%%ebx)\n\t"
"	fstps 120(%%ecx)\n\t"

/* Phase 5*/

"	flds   32(%%ecx)\n\t"
"	fadds  36(%%ecx)\n\t"
"	fstps  32(%%edx)\n\t"

"	flds   32(%%ecx)\n\t"
"	fsubs  36(%%ecx)\n\t"
"	fmuls 120(%%ebx)\n\t"
"	fstps  36(%%edx)\n\t"

"	flds   44(%%ecx)\n\t"
"	fsubs  40(%%ecx)\n\t"
"	fmuls 120(%%ebx)\n\t"
"	fsts   44(%%edx)\n\t"
"	fadds  40(%%ecx)\n\t"
"	fadds  44(%%ecx)\n\t"
"	fstps  40(%%edx)\n\t"

"	flds   48(%%ecx)\n\t"
"	fsubs  52(%%ecx)\n\t"
"	fmuls 120(%%ebx)\n\t"

"	flds   60(%%ecx)\n\t"
"	fsubs  56(%%ecx)\n\t"
"	fmuls 120(%%ebx)\n\t"
"	fld      %%st(0)\n\t"
"	fadds  56(%%ecx)\n\t"
"	fadds  60(%%ecx)\n\t"
"	fld      %%st(0)\n\t"
"	fadds  48(%%ecx)\n\t"
"	fadds  52(%%ecx)\n\t"
"	fstps  48(%%edx)\n\t"
"	fadd     %%st(2)\n\t"
"	fstps  56(%%edx)\n\t"
"	fsts   60(%%edx)\n\t"
"	faddp    %%st(1)\n\t"
"	fstps  52(%%edx)\n\t"

"	flds   64(%%ecx)\n\t"
"	fadds  68(%%ecx)\n\t"
"	fstps  64(%%edx)\n\t"

"	flds   64(%%ecx)\n\t"
"	fsubs  68(%%ecx)\n\t"
"	fmuls 120(%%ebx)\n\t"
"	fstps  68(%%edx)\n\t"

"	flds   76(%%ecx)\n\t"
"	fsubs  72(%%ecx)\n\t"
"	fmuls 120(%%ebx)\n\t"
"	fsts   76(%%edx)\n\t"
"	fadds  72(%%ecx)\n\t"
"	fadds  76(%%ecx)\n\t"
"	fstps  72(%%edx)\n\t"

"	flds   92(%%ecx)\n\t"
"	fsubs  88(%%ecx)\n\t"
"	fmuls 120(%%ebx)\n\t"
"	fsts   92(%%edx)\n\t"
"	fadds  92(%%ecx)\n\t"
"	fadds  88(%%ecx)\n\t"
"	fld      %%st(0)\n\t"
"	fadds  80(%%ecx)\n\t"
"	fadds  84(%%ecx)\n\t"
"	fstps  80(%%edx)\n\t"

"	flds   80(%%ecx)\n\t"
"	fsubs  84(%%ecx)\n\t"
"	fmuls 120(%%ebx)\n\t"
"	fadd  %%st(0), %%st(1)\n\t"
"	fadds 92(%%edx)\n\t"
"	fstps 84(%%edx)\n\t"
"	fstps 88(%%edx)\n\t"

"	flds   96(%%ecx)\n\t"
"	fadds 100(%%ecx)\n\t"
"	fstps  96(%%edx)\n\t"

"	flds   96(%%ecx)\n\t"
"	fsubs 100(%%ecx)\n\t"
"	fmuls 120(%%ebx)\n\t"
"	fstps 100(%%edx)\n\t"

"	flds  108(%%ecx)\n\t"
"	fsubs 104(%%ecx)\n\t"
"	fmuls 120(%%ebx)\n\t"
"	fsts  108(%%edx)\n\t"
"	fadds 104(%%ecx)\n\t"
"	fadds 108(%%ecx)\n\t"
"	fstps 104(%%edx)\n\t"

"	flds  124(%%ecx)\n\t"
"	fsubs 120(%%ecx)\n\t"
"	fmuls 120(%%ebx)\n\t"
"	fsts  124(%%edx)\n\t"
"	fadds 120(%%ecx)\n\t"
"	fadds 124(%%ecx)\n\t"
"	fld      %%st(0)\n\t"
"	fadds 112(%%ecx)\n\t"
"	fadds 116(%%ecx)\n\t"
"	fstps 112(%%edx)\n\t"

"	flds  112(%%ecx)\n\t"
"	fsubs 116(%%ecx)\n\t"
"	fmuls 120(%%ebx)\n\t"
"	fadd  %%st(0),%%st(1)\n\t"
"	fadds 124(%%edx)\n\t"
"	fstps 116(%%edx)\n\t"
"	fstps 120(%%edx)\n\t"
"	jnz .L01\n\t"

/* Phase 6*/

"	flds      (%%ecx)\n\t"
"	fadds    4(%%ecx)\n\t"
"	fstps 1024(%%esi)\n\t"

"	flds      (%%ecx)\n\t"
"	fsubs    4(%%ecx)\n\t"
"	fmuls  120(%%ebx)\n\t"
"	fsts      (%%esi)\n\t"
"	fstps     (%%edi)\n\t"

"	flds   12(%%ecx)\n\t"
"	fsubs   8(%%ecx)\n\t"
"	fmuls 120(%%ebx)\n\t"
"	fsts  512(%%edi)\n\t"
"	fadds  12(%%ecx)\n\t"
"	fadds   8(%%ecx)\n\t"
"	fstps 512(%%esi)\n\t"

"	flds   16(%%ecx)\n\t"
"	fsubs  20(%%ecx)\n\t"
"	fmuls 120(%%ebx)\n\t"

"	flds   28(%%ecx)\n\t"
"	fsubs  24(%%ecx)\n\t"
"	fmuls 120(%%ebx)\n\t"
"	fsts  768(%%edi)\n\t"
"	fld      %%st(0)\n\t"
"	fadds  24(%%ecx)\n\t"
"	fadds  28(%%ecx)\n\t"
"	fld      %%st(0)\n\t"
"	fadds  16(%%ecx)\n\t"
"	fadds  20(%%ecx)\n\t"
"	fstps 768(%%esi)\n\t"
"	fadd     %%st(2)\n\t"
"	fstps 256(%%esi)\n\t"
"	faddp    %%st(1)\n\t"
"	fstps 256(%%edi)\n\t"

/* Phase 7*/

"	flds   32(%%edx)\n\t"
"	fadds  48(%%edx)\n\t"
"	fstps 896(%%esi)\n\t"

"	flds   48(%%edx)\n\t"
"	fadds  40(%%edx)\n\t"
"	fstps 640(%%esi)\n\t"

"	flds   40(%%edx)\n\t"
"	fadds  56(%%edx)\n\t"
"	fstps 384(%%esi)\n\t"

"	flds   56(%%edx)\n\t"
"	fadds  36(%%edx)\n\t"
"	fstps 128(%%esi)\n\t"

"	flds   36(%%edx)\n\t"
"	fadds  52(%%edx)\n\t"
"	fstps 128(%%edi)\n\t"

"	flds   52(%%edx)\n\t"
"	fadds  44(%%edx)\n\t"
"	fstps 384(%%edi)\n\t"

"	flds   60(%%edx)\n\t"
"	fsts  896(%%edi)\n\t"
"	fadds  44(%%edx)\n\t"
"	fstps 640(%%edi)\n\t"

"	flds   96(%%edx)\n\t"
"	fadds 112(%%edx)\n\t"
"	fld      %%st(0)\n\t"
"	fadds  64(%%edx)\n\t"
"	fstps 960(%%esi)\n\t"
"	fadds  80(%%edx)\n\t"
"	fstps 832(%%esi)\n\t"

"	flds  112(%%edx)\n\t"
"	fadds 104(%%edx)\n\t"
"	fld      %%st(0)\n\t"
"	fadds  80(%%edx)\n\t"
"	fstps 704(%%esi)\n\t"
"	fadds  72(%%edx)\n\t"
"	fstps 576(%%esi)\n\t"

"	flds  104(%%edx)\n\t"
"	fadds 120(%%edx)\n\t"
"	fld      %%st(0)\n\t"
"	fadds  72(%%edx)\n\t"
"	fstps 448(%%esi)\n\t"
"	fadds  88(%%edx)\n\t"
"	fstps 320(%%esi)\n\t"

"	flds  120(%%edx)\n\t"
"	fadds 100(%%edx)\n\t"
"	fld      %%st(0)\n\t"
"	fadds  88(%%edx)\n\t"
"	fstps 192(%%esi)\n\t"
"	fadds  68(%%edx)\n\t"
"	fstps  64(%%esi)\n\t"

"	flds  100(%%edx)\n\t"
"	fadds 116(%%edx)\n\t"
"	fld      %%st(0)\n\t"
"	fadds  68(%%edx)\n\t"
"	fstps  64(%%edi)\n\t"
"	fadds  84(%%edx)\n\t"
"	fstps 192(%%edi)\n\t"

"	flds  116(%%edx)\n\t"
"	fadds 108(%%edx)\n\t"
"	fld      %%st(0)\n\t"
"	fadds  84(%%edx)\n\t"
"	fstps 320(%%edi)\n\t"
"	fadds  76(%%edx)\n\t"
"	fstps 448(%%edi)\n\t"

"	flds  108(%%edx)\n\t"
"	fadds 124(%%edx)\n\t"
"	fld      %%st(0)\n\t"
"	fadds  76(%%edx)\n\t"
"	fstps 576(%%edi)\n\t"
"	fadds  92(%%edx)\n\t"
"	fstps 704(%%edi)\n\t"

"	flds  124(%%edx)\n\t"
"	fsts  960(%%edi)\n\t"
"	fadds  92(%%edx)\n\t"
"	fstps 832(%%edi)\n\t"
"	jmp	.L_bye\n\t"
".L01:\n\t"
/* Phase 8*/

"	flds      (%%ecx)\n\t"
"	fadds    4(%%ecx)\n\t"
"	fistp  512(%%esi)\n\t"

"	flds      (%%ecx)\n\t"
"	fsubs    4(%%ecx)\n\t"
"	fmuls  120(%%ebx)\n\t"

"	fistp     (%%esi)\n\t"


"	flds    12(%%ecx)\n\t"
"	fsubs    8(%%ecx)\n\t"
"	fmuls  120(%%ebx)\n\t"
"	fist   256(%%edi)\n\t"
"	fadds   12(%%ecx)\n\t"
"	fadds    8(%%ecx)\n\t"
"	fistp  256(%%esi)\n\t"

"	flds   16(%%ecx)\n\t"
"	fsubs  20(%%ecx)\n\t"
"	fmuls 120(%%ebx)\n\t"

"	flds   28(%%ecx)\n\t"
"	fsubs  24(%%ecx)\n\t"
"	fmuls 120(%%ebx)\n\t"
"	fist  384(%%edi)\n\t"
"	fld      %%st(0)\n\t"
"	fadds  24(%%ecx)\n\t"
"	fadds  28(%%ecx)\n\t"
"	fld      %%st(0)\n\t"
"	fadds  16(%%ecx)\n\t"
"	fadds  20(%%ecx)\n\t"
"	fistp  384(%%esi)\n\t"
"	fadd     %%st(2)\n\t"
"	fistp  128(%%esi)\n\t"
"	faddp    %%st(1)\n\t"
"	fistp  128(%%edi)\n\t"

/* Phase 9*/

"	flds    32(%%edx)\n\t"
"	fadds   48(%%edx)\n\t"
"	fistp  448(%%esi)\n\t"

"	flds   48(%%edx)\n\t"
"	fadds  40(%%edx)\n\t"
"	fistp 320(%%esi)\n\t"

"	flds   40(%%edx)\n\t"
"	fadds  56(%%edx)\n\t"
"	fistp 192(%%esi)\n\t"

"	flds   56(%%edx)\n\t"
"	fadds  36(%%edx)\n\t"
"	fistp  64(%%esi)\n\t"

"	flds   36(%%edx)\n\t"
"	fadds  52(%%edx)\n\t"
"	fistp  64(%%edi)\n\t"

"	flds   52(%%edx)\n\t"
"	fadds  44(%%edx)\n\t"
"	fistp 192(%%edi)\n\t"

"	flds   60(%%edx)\n\t"
"	fist   448(%%edi)\n\t"
"	fadds  44(%%edx)\n\t"
"	fistp 320(%%edi)\n\t"

"	flds   96(%%edx)\n\t"
"	fadds 112(%%edx)\n\t"
"	fld      %%st(0)\n\t"
"	fadds  64(%%edx)\n\t"
"	fistp 480(%%esi)\n\t"
"	fadds  80(%%edx)\n\t"
"	fistp 416(%%esi)\n\t"

"	flds  112(%%edx)\n\t"
"	fadds 104(%%edx)\n\t"
"	fld      %%st(0)\n\t"
"	fadds  80(%%edx)\n\t"
"	fistp 352(%%esi)\n\t"
"	fadds  72(%%edx)\n\t"
"	fistp 288(%%esi)\n\t"

"	flds  104(%%edx)\n\t"
"	fadds 120(%%edx)\n\t"
"	fld      %%st(0)\n\t"
"	fadds  72(%%edx)\n\t"
"	fistp 224(%%esi)\n\t"
"	fadds  88(%%edx)\n\t"
"	fistp 160(%%esi)\n\t"

"	flds  120(%%edx)\n\t"
"	fadds 100(%%edx)\n\t"
"	fld      %%st(0)\n\t"
"	fadds  88(%%edx)\n\t"
"	fistp  96(%%esi)\n\t"
"	fadds  68(%%edx)\n\t"
"	fistp  32(%%esi)\n\t"

"	flds  100(%%edx)\n\t"
"	fadds 116(%%edx)\n\t"
"	fld      %%st(0)\n\t"
"	fadds  68(%%edx)\n\t"
"	fistp  32(%%edi)\n\t"
"	fadds  84(%%edx)\n\t"
"	fistp  96(%%edi)\n\t"

"	flds  116(%%edx)\n\t"
"	fadds 108(%%edx)\n\t"
"	fld      %%st(0)\n\t"
"	fadds  84(%%edx)\n\t"
"	fistp 160(%%edi)\n\t"
"	fadds  76(%%edx)\n\t"
"	fistp 224(%%edi)\n\t"

"	flds  108(%%edx)\n\t"
"	fadds 124(%%edx)\n\t"
"	fld      %%st(0)\n\t"
"	fadds  76(%%edx)\n\t"
"	fistp 288(%%edi)\n\t"
"	fadds  92(%%edx)\n\t"
"	fistp 352(%%edi)\n\t"

"	flds  124(%%edx)\n\t"
"	fist  480(%%edi)\n\t"
"	fadds  92(%%edx)\n\t"
"	fistp 416(%%edi)\n\t"
"	movsw\n\t"
".L_bye:"
	:
	:"m"(a),"m"(b),"m"(c),"m"(tmp[0])
	:"memory","%eax","%ebx","%ecx","%edx","%esi","%edi");
}
