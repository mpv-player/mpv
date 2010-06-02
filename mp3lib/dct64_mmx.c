/*
* This code was taken from http://www.mpg123.org
* See ChangeLog of mpg123-0.59s-pre.1 for detail
* Applied to mplayer by Nick Kurshev <nickols_k@mail.ru>
*/
#include "config.h"
#include "mangle.h"
#include "mpg123.h"
#include "libavutil/x86_cpu.h"

void dct64_MMX(short *a,short *b,real *c)
{
    char tmp[256];
    __asm__ volatile(
"       mov %2,%%"REG_a"\n\t"
/* Phase 1*/
"       flds     (%%"REG_a")\n\t"
"       lea 128+%3,%%"REG_d"\n\t"
"       fadds 124(%%"REG_a")\n\t"
"       mov %0,%%"REG_S"\n\t"
"       fstps    (%%"REG_d")\n\t"
"       mov %1,%%"REG_D"\n\t"

"       flds    4(%%"REG_a")\n\t"
"       mov $"MANGLE(costab_mmx)",%%"REG_b"\n\t"
"       fadds 120(%%"REG_a")\n\t"
"       or %%"REG_c",%%"REG_c"\n\t"
"       fstps   4(%%"REG_d")\n\t"

"       flds     (%%"REG_a")\n\t"
"       lea %3,%%"REG_c"\n\t"
"       fsubs 124(%%"REG_a")\n\t"
"       fmuls    (%%"REG_b")\n\t"
"       fstps 124(%%"REG_d")\n\t"

"       flds    4(%%"REG_a")\n\t"
"       fsubs 120(%%"REG_a")\n\t"
"       fmuls   4(%%"REG_b")\n\t"
"       fstps 120(%%"REG_d")\n\t"

"       flds    8(%%"REG_a")\n\t"
"       fadds 116(%%"REG_a")\n\t"
"       fstps   8(%%"REG_d")\n\t"

"       flds   12(%%"REG_a")\n\t"
"       fadds 112(%%"REG_a")\n\t"
"       fstps  12(%%"REG_d")\n\t"

"       flds    8(%%"REG_a")\n\t"
"       fsubs 116(%%"REG_a")\n\t"
"       fmuls   8(%%"REG_b")\n\t"
"       fstps 116(%%"REG_d")\n\t"

"       flds   12(%%"REG_a")\n\t"
"       fsubs 112(%%"REG_a")\n\t"
"       fmuls  12(%%"REG_b")\n\t"
"       fstps 112(%%"REG_d")\n\t"

"       flds   16(%%"REG_a")\n\t"
"       fadds 108(%%"REG_a")\n\t"
"       fstps  16(%%"REG_d")\n\t"

"       flds   20(%%"REG_a")\n\t"
"       fadds 104(%%"REG_a")\n\t"
"       fstps  20(%%"REG_d")\n\t"

"       flds   16(%%"REG_a")\n\t"
"       fsubs 108(%%"REG_a")\n\t"
"       fmuls  16(%%"REG_b")\n\t"
"       fstps 108(%%"REG_d")\n\t"

"       flds   20(%%"REG_a")\n\t"
"       fsubs 104(%%"REG_a")\n\t"
"       fmuls  20(%%"REG_b")\n\t"
"       fstps 104(%%"REG_d")\n\t"

"       flds   24(%%"REG_a")\n\t"
"       fadds 100(%%"REG_a")\n\t"
"       fstps  24(%%"REG_d")\n\t"

"       flds   28(%%"REG_a")\n\t"
"       fadds  96(%%"REG_a")\n\t"
"       fstps  28(%%"REG_d")\n\t"

"       flds   24(%%"REG_a")\n\t"
"       fsubs 100(%%"REG_a")\n\t"
"       fmuls  24(%%"REG_b")\n\t"
"       fstps 100(%%"REG_d")\n\t"

"       flds   28(%%"REG_a")\n\t"
"       fsubs  96(%%"REG_a")\n\t"
"       fmuls  28(%%"REG_b")\n\t"
"       fstps  96(%%"REG_d")\n\t"

"       flds   32(%%"REG_a")\n\t"
"       fadds  92(%%"REG_a")\n\t"
"       fstps  32(%%"REG_d")\n\t"

"       flds   36(%%"REG_a")\n\t"
"       fadds  88(%%"REG_a")\n\t"
"       fstps  36(%%"REG_d")\n\t"

"       flds   32(%%"REG_a")\n\t"
"       fsubs  92(%%"REG_a")\n\t"
"       fmuls  32(%%"REG_b")\n\t"
"       fstps  92(%%"REG_d")\n\t"

"       flds   36(%%"REG_a")\n\t"
"       fsubs  88(%%"REG_a")\n\t"
"       fmuls  36(%%"REG_b")\n\t"
"       fstps  88(%%"REG_d")\n\t"

"       flds   40(%%"REG_a")\n\t"
"       fadds  84(%%"REG_a")\n\t"
"       fstps  40(%%"REG_d")\n\t"

"       flds   44(%%"REG_a")\n\t"
"       fadds  80(%%"REG_a")\n\t"
"       fstps  44(%%"REG_d")\n\t"

"       flds   40(%%"REG_a")\n\t"
"       fsubs  84(%%"REG_a")\n\t"
"       fmuls  40(%%"REG_b")\n\t"
"       fstps  84(%%"REG_d")\n\t"

"       flds   44(%%"REG_a")\n\t"
"       fsubs  80(%%"REG_a")\n\t"
"       fmuls  44(%%"REG_b")\n\t"
"       fstps  80(%%"REG_d")\n\t"

"       flds   48(%%"REG_a")\n\t"
"       fadds  76(%%"REG_a")\n\t"
"       fstps  48(%%"REG_d")\n\t"

"       flds   52(%%"REG_a")\n\t"
"       fadds  72(%%"REG_a")\n\t"
"       fstps  52(%%"REG_d")\n\t"

"       flds   48(%%"REG_a")\n\t"
"       fsubs  76(%%"REG_a")\n\t"
"       fmuls  48(%%"REG_b")\n\t"
"       fstps  76(%%"REG_d")\n\t"

"       flds   52(%%"REG_a")\n\t"
"       fsubs  72(%%"REG_a")\n\t"
"       fmuls  52(%%"REG_b")\n\t"
"       fstps  72(%%"REG_d")\n\t"

"       flds   56(%%"REG_a")\n\t"
"       fadds  68(%%"REG_a")\n\t"
"       fstps  56(%%"REG_d")\n\t"

"       flds   60(%%"REG_a")\n\t"
"       fadds  64(%%"REG_a")\n\t"
"       fstps  60(%%"REG_d")\n\t"

"       flds   56(%%"REG_a")\n\t"
"       fsubs  68(%%"REG_a")\n\t"
"       fmuls  56(%%"REG_b")\n\t"
"       fstps  68(%%"REG_d")\n\t"

"       flds   60(%%"REG_a")\n\t"
"       fsubs  64(%%"REG_a")\n\t"
"       fmuls  60(%%"REG_b")\n\t"
"       fstps  64(%%"REG_d")\n\t"

/* Phase 2*/

"       flds     (%%"REG_d")\n\t"
"       fadds  60(%%"REG_d")\n\t"
"       fstps    (%%"REG_c")\n\t"

"       flds    4(%%"REG_d")\n\t"
"       fadds  56(%%"REG_d")\n\t"
"       fstps   4(%%"REG_c")\n\t"

"       flds     (%%"REG_d")\n\t"
"       fsubs  60(%%"REG_d")\n\t"
"       fmuls  64(%%"REG_b")\n\t"
"       fstps  60(%%"REG_c")\n\t"

"       flds    4(%%"REG_d")\n\t"
"       fsubs  56(%%"REG_d")\n\t"
"       fmuls  68(%%"REG_b")\n\t"
"       fstps  56(%%"REG_c")\n\t"

"       flds    8(%%"REG_d")\n\t"
"       fadds  52(%%"REG_d")\n\t"
"       fstps   8(%%"REG_c")\n\t"

"       flds   12(%%"REG_d")\n\t"
"       fadds  48(%%"REG_d")\n\t"
"       fstps  12(%%"REG_c")\n\t"

"       flds    8(%%"REG_d")\n\t"
"       fsubs  52(%%"REG_d")\n\t"
"       fmuls  72(%%"REG_b")\n\t"
"       fstps  52(%%"REG_c")\n\t"

"       flds   12(%%"REG_d")\n\t"
"       fsubs  48(%%"REG_d")\n\t"
"       fmuls  76(%%"REG_b")\n\t"
"       fstps  48(%%"REG_c")\n\t"

"       flds   16(%%"REG_d")\n\t"
"       fadds  44(%%"REG_d")\n\t"
"       fstps  16(%%"REG_c")\n\t"

"       flds   20(%%"REG_d")\n\t"
"       fadds  40(%%"REG_d")\n\t"
"       fstps  20(%%"REG_c")\n\t"

"       flds   16(%%"REG_d")\n\t"
"       fsubs  44(%%"REG_d")\n\t"
"       fmuls  80(%%"REG_b")\n\t"
"       fstps  44(%%"REG_c")\n\t"

"       flds   20(%%"REG_d")\n\t"
"       fsubs  40(%%"REG_d")\n\t"
"       fmuls  84(%%"REG_b")\n\t"
"       fstps  40(%%"REG_c")\n\t"

"       flds   24(%%"REG_d")\n\t"
"       fadds  36(%%"REG_d")\n\t"
"       fstps  24(%%"REG_c")\n\t"

"       flds   28(%%"REG_d")\n\t"
"       fadds  32(%%"REG_d")\n\t"
"       fstps  28(%%"REG_c")\n\t"

"       flds   24(%%"REG_d")\n\t"
"       fsubs  36(%%"REG_d")\n\t"
"       fmuls  88(%%"REG_b")\n\t"
"       fstps  36(%%"REG_c")\n\t"

"       flds   28(%%"REG_d")\n\t"
"       fsubs  32(%%"REG_d")\n\t"
"       fmuls  92(%%"REG_b")\n\t"
"       fstps  32(%%"REG_c")\n\t"

/* Phase 3*/

"       flds   64(%%"REG_d")\n\t"
"       fadds 124(%%"REG_d")\n\t"
"       fstps  64(%%"REG_c")\n\t"

"       flds   68(%%"REG_d")\n\t"
"       fadds 120(%%"REG_d")\n\t"
"       fstps  68(%%"REG_c")\n\t"

"       flds  124(%%"REG_d")\n\t"
"       fsubs  64(%%"REG_d")\n\t"
"       fmuls  64(%%"REG_b")\n\t"
"       fstps 124(%%"REG_c")\n\t"

"       flds  120(%%"REG_d")\n\t"
"       fsubs  68(%%"REG_d")\n\t"
"       fmuls  68(%%"REG_b")\n\t"
"       fstps 120(%%"REG_c")\n\t"

"       flds   72(%%"REG_d")\n\t"
"       fadds 116(%%"REG_d")\n\t"
"       fstps  72(%%"REG_c")\n\t"

"       flds   76(%%"REG_d")\n\t"
"       fadds 112(%%"REG_d")\n\t"
"       fstps  76(%%"REG_c")\n\t"

"       flds  116(%%"REG_d")\n\t"
"       fsubs  72(%%"REG_d")\n\t"
"       fmuls  72(%%"REG_b")\n\t"
"       fstps 116(%%"REG_c")\n\t"

"       flds  112(%%"REG_d")\n\t"
"       fsubs  76(%%"REG_d")\n\t"
"       fmuls  76(%%"REG_b")\n\t"
"       fstps 112(%%"REG_c")\n\t"

"       flds   80(%%"REG_d")\n\t"
"       fadds 108(%%"REG_d")\n\t"
"       fstps  80(%%"REG_c")\n\t"

"       flds   84(%%"REG_d")\n\t"
"       fadds 104(%%"REG_d")\n\t"
"       fstps  84(%%"REG_c")\n\t"

"       flds  108(%%"REG_d")\n\t"
"       fsubs  80(%%"REG_d")\n\t"
"       fmuls  80(%%"REG_b")\n\t"
"       fstps 108(%%"REG_c")\n\t"

"       flds  104(%%"REG_d")\n\t"
"       fsubs  84(%%"REG_d")\n\t"
"       fmuls  84(%%"REG_b")\n\t"
"       fstps 104(%%"REG_c")\n\t"

"       flds   88(%%"REG_d")\n\t"
"       fadds 100(%%"REG_d")\n\t"
"       fstps  88(%%"REG_c")\n\t"

"       flds   92(%%"REG_d")\n\t"
"       fadds  96(%%"REG_d")\n\t"
"       fstps  92(%%"REG_c")\n\t"

"       flds  100(%%"REG_d")\n\t"
"       fsubs  88(%%"REG_d")\n\t"
"       fmuls  88(%%"REG_b")\n\t"
"       fstps 100(%%"REG_c")\n\t"

"       flds   96(%%"REG_d")\n\t"
"       fsubs  92(%%"REG_d")\n\t"
"       fmuls  92(%%"REG_b")\n\t"
"       fstps  96(%%"REG_c")\n\t"

/* Phase 4*/

"       flds     (%%"REG_c")\n\t"
"       fadds  28(%%"REG_c")\n\t"
"       fstps    (%%"REG_d")\n\t"

"       flds     (%%"REG_c")\n\t"
"       fsubs  28(%%"REG_c")\n\t"
"       fmuls  96(%%"REG_b")\n\t"
"       fstps  28(%%"REG_d")\n\t"

"       flds    4(%%"REG_c")\n\t"
"       fadds  24(%%"REG_c")\n\t"
"       fstps   4(%%"REG_d")\n\t"

"       flds    4(%%"REG_c")\n\t"
"       fsubs  24(%%"REG_c")\n\t"
"       fmuls 100(%%"REG_b")\n\t"
"       fstps  24(%%"REG_d")\n\t"

"       flds    8(%%"REG_c")\n\t"
"       fadds  20(%%"REG_c")\n\t"
"       fstps   8(%%"REG_d")\n\t"

"       flds    8(%%"REG_c")\n\t"
"       fsubs  20(%%"REG_c")\n\t"
"       fmuls 104(%%"REG_b")\n\t"
"       fstps  20(%%"REG_d")\n\t"

"       flds   12(%%"REG_c")\n\t"
"       fadds  16(%%"REG_c")\n\t"
"       fstps  12(%%"REG_d")\n\t"

"       flds   12(%%"REG_c")\n\t"
"       fsubs  16(%%"REG_c")\n\t"
"       fmuls 108(%%"REG_b")\n\t"
"       fstps  16(%%"REG_d")\n\t"

"       flds   32(%%"REG_c")\n\t"
"       fadds  60(%%"REG_c")\n\t"
"       fstps  32(%%"REG_d")\n\t"

"       flds   60(%%"REG_c")\n\t"
"       fsubs  32(%%"REG_c")\n\t"
"       fmuls  96(%%"REG_b")\n\t"
"       fstps  60(%%"REG_d")\n\t"

"       flds   36(%%"REG_c")\n\t"
"       fadds  56(%%"REG_c")\n\t"
"       fstps  36(%%"REG_d")\n\t"

"       flds   56(%%"REG_c")\n\t"
"       fsubs  36(%%"REG_c")\n\t"
"       fmuls 100(%%"REG_b")\n\t"
"       fstps  56(%%"REG_d")\n\t"

"       flds   40(%%"REG_c")\n\t"
"       fadds  52(%%"REG_c")\n\t"
"       fstps  40(%%"REG_d")\n\t"

"       flds   52(%%"REG_c")\n\t"
"       fsubs  40(%%"REG_c")\n\t"
"       fmuls 104(%%"REG_b")\n\t"
"       fstps  52(%%"REG_d")\n\t"

"       flds   44(%%"REG_c")\n\t"
"       fadds  48(%%"REG_c")\n\t"
"       fstps  44(%%"REG_d")\n\t"

"       flds   48(%%"REG_c")\n\t"
"       fsubs  44(%%"REG_c")\n\t"
"       fmuls 108(%%"REG_b")\n\t"
"       fstps  48(%%"REG_d")\n\t"

"       flds   64(%%"REG_c")\n\t"
"       fadds  92(%%"REG_c")\n\t"
"       fstps  64(%%"REG_d")\n\t"

"       flds   64(%%"REG_c")\n\t"
"       fsubs  92(%%"REG_c")\n\t"
"       fmuls  96(%%"REG_b")\n\t"
"       fstps  92(%%"REG_d")\n\t"

"       flds   68(%%"REG_c")\n\t"
"       fadds  88(%%"REG_c")\n\t"
"       fstps  68(%%"REG_d")\n\t"

"       flds   68(%%"REG_c")\n\t"
"       fsubs  88(%%"REG_c")\n\t"
"       fmuls 100(%%"REG_b")\n\t"
"       fstps  88(%%"REG_d")\n\t"

"       flds   72(%%"REG_c")\n\t"
"       fadds  84(%%"REG_c")\n\t"
"       fstps  72(%%"REG_d")\n\t"

"       flds   72(%%"REG_c")\n\t"
"       fsubs  84(%%"REG_c")\n\t"
"       fmuls 104(%%"REG_b")\n\t"
"       fstps  84(%%"REG_d")\n\t"

"       flds   76(%%"REG_c")\n\t"
"       fadds  80(%%"REG_c")\n\t"
"       fstps  76(%%"REG_d")\n\t"

"       flds   76(%%"REG_c")\n\t"
"       fsubs  80(%%"REG_c")\n\t"
"       fmuls 108(%%"REG_b")\n\t"
"       fstps  80(%%"REG_d")\n\t"

"       flds   96(%%"REG_c")\n\t"
"       fadds 124(%%"REG_c")\n\t"
"       fstps  96(%%"REG_d")\n\t"

"       flds  124(%%"REG_c")\n\t"
"       fsubs  96(%%"REG_c")\n\t"
"       fmuls  96(%%"REG_b")\n\t"
"       fstps 124(%%"REG_d")\n\t"

"       flds  100(%%"REG_c")\n\t"
"       fadds 120(%%"REG_c")\n\t"
"       fstps 100(%%"REG_d")\n\t"

"       flds  120(%%"REG_c")\n\t"
"       fsubs 100(%%"REG_c")\n\t"
"       fmuls 100(%%"REG_b")\n\t"
"       fstps 120(%%"REG_d")\n\t"

"       flds  104(%%"REG_c")\n\t"
"       fadds 116(%%"REG_c")\n\t"
"       fstps 104(%%"REG_d")\n\t"

"       flds  116(%%"REG_c")\n\t"
"       fsubs 104(%%"REG_c")\n\t"
"       fmuls 104(%%"REG_b")\n\t"
"       fstps 116(%%"REG_d")\n\t"

"       flds  108(%%"REG_c")\n\t"
"       fadds 112(%%"REG_c")\n\t"
"       fstps 108(%%"REG_d")\n\t"

"       flds  112(%%"REG_c")\n\t"
"       fsubs 108(%%"REG_c")\n\t"
"       fmuls 108(%%"REG_b")\n\t"
"       fstps 112(%%"REG_d")\n\t"

"       flds     (%%"REG_d")\n\t"
"       fadds  12(%%"REG_d")\n\t"
"       fstps    (%%"REG_c")\n\t"

"       flds     (%%"REG_d")\n\t"
"       fsubs  12(%%"REG_d")\n\t"
"       fmuls 112(%%"REG_b")\n\t"
"       fstps  12(%%"REG_c")\n\t"

"       flds    4(%%"REG_d")\n\t"
"       fadds   8(%%"REG_d")\n\t"
"       fstps   4(%%"REG_c")\n\t"

"       flds    4(%%"REG_d")\n\t"
"       fsubs   8(%%"REG_d")\n\t"
"       fmuls 116(%%"REG_b")\n\t"
"       fstps   8(%%"REG_c")\n\t"

"       flds   16(%%"REG_d")\n\t"
"       fadds  28(%%"REG_d")\n\t"
"       fstps  16(%%"REG_c")\n\t"

"       flds   28(%%"REG_d")\n\t"
"       fsubs  16(%%"REG_d")\n\t"
"       fmuls 112(%%"REG_b")\n\t"
"       fstps  28(%%"REG_c")\n\t"

"       flds   20(%%"REG_d")\n\t"
"       fadds  24(%%"REG_d")\n\t"
"       fstps  20(%%"REG_c")\n\t"

"       flds   24(%%"REG_d")\n\t"
"       fsubs  20(%%"REG_d")\n\t"
"       fmuls 116(%%"REG_b")\n\t"
"       fstps  24(%%"REG_c")\n\t"

"       flds   32(%%"REG_d")\n\t"
"       fadds  44(%%"REG_d")\n\t"
"       fstps  32(%%"REG_c")\n\t"

"       flds   32(%%"REG_d")\n\t"
"       fsubs  44(%%"REG_d")\n\t"
"       fmuls 112(%%"REG_b")\n\t"
"       fstps  44(%%"REG_c")\n\t"

"       flds   36(%%"REG_d")\n\t"
"       fadds  40(%%"REG_d")\n\t"
"       fstps  36(%%"REG_c")\n\t"

"       flds   36(%%"REG_d")\n\t"
"       fsubs  40(%%"REG_d")\n\t"
"       fmuls 116(%%"REG_b")\n\t"
"       fstps  40(%%"REG_c")\n\t"

"       flds   48(%%"REG_d")\n\t"
"       fadds  60(%%"REG_d")\n\t"
"       fstps  48(%%"REG_c")\n\t"

"       flds   60(%%"REG_d")\n\t"
"       fsubs  48(%%"REG_d")\n\t"
"       fmuls 112(%%"REG_b")\n\t"
"       fstps  60(%%"REG_c")\n\t"

"       flds   52(%%"REG_d")\n\t"
"       fadds  56(%%"REG_d")\n\t"
"       fstps  52(%%"REG_c")\n\t"

"       flds   56(%%"REG_d")\n\t"
"       fsubs  52(%%"REG_d")\n\t"
"       fmuls 116(%%"REG_b")\n\t"
"       fstps  56(%%"REG_c")\n\t"

"       flds   64(%%"REG_d")\n\t"
"       fadds  76(%%"REG_d")\n\t"
"       fstps  64(%%"REG_c")\n\t"

"       flds   64(%%"REG_d")\n\t"
"       fsubs  76(%%"REG_d")\n\t"
"       fmuls 112(%%"REG_b")\n\t"
"       fstps  76(%%"REG_c")\n\t"

"       flds   68(%%"REG_d")\n\t"
"       fadds  72(%%"REG_d")\n\t"
"       fstps  68(%%"REG_c")\n\t"

"       flds   68(%%"REG_d")\n\t"
"       fsubs  72(%%"REG_d")\n\t"
"       fmuls 116(%%"REG_b")\n\t"
"       fstps  72(%%"REG_c")\n\t"

"       flds   80(%%"REG_d")\n\t"
"       fadds  92(%%"REG_d")\n\t"
"       fstps  80(%%"REG_c")\n\t"

"       flds   92(%%"REG_d")\n\t"
"       fsubs  80(%%"REG_d")\n\t"
"       fmuls 112(%%"REG_b")\n\t"
"       fstps  92(%%"REG_c")\n\t"

"       flds   84(%%"REG_d")\n\t"
"       fadds  88(%%"REG_d")\n\t"
"       fstps  84(%%"REG_c")\n\t"

"       flds   88(%%"REG_d")\n\t"
"       fsubs  84(%%"REG_d")\n\t"
"       fmuls 116(%%"REG_b")\n\t"
"       fstps  88(%%"REG_c")\n\t"

"       flds   96(%%"REG_d")\n\t"
"       fadds 108(%%"REG_d")\n\t"
"       fstps  96(%%"REG_c")\n\t"

"       flds   96(%%"REG_d")\n\t"
"       fsubs 108(%%"REG_d")\n\t"
"       fmuls 112(%%"REG_b")\n\t"
"       fstps 108(%%"REG_c")\n\t"

"       flds  100(%%"REG_d")\n\t"
"       fadds 104(%%"REG_d")\n\t"
"       fstps 100(%%"REG_c")\n\t"

"       flds  100(%%"REG_d")\n\t"
"       fsubs 104(%%"REG_d")\n\t"
"       fmuls 116(%%"REG_b")\n\t"
"       fstps 104(%%"REG_c")\n\t"

"       flds  112(%%"REG_d")\n\t"
"       fadds 124(%%"REG_d")\n\t"
"       fstps 112(%%"REG_c")\n\t"

"       flds  124(%%"REG_d")\n\t"
"       fsubs 112(%%"REG_d")\n\t"
"       fmuls 112(%%"REG_b")\n\t"
"       fstps 124(%%"REG_c")\n\t"

"       flds  116(%%"REG_d")\n\t"
"       fadds 120(%%"REG_d")\n\t"
"       fstps 116(%%"REG_c")\n\t"

"       flds  120(%%"REG_d")\n\t"
"       fsubs 116(%%"REG_d")\n\t"
"       fmuls 116(%%"REG_b")\n\t"
"       fstps 120(%%"REG_c")\n\t"

/* Phase 5*/

"       flds   32(%%"REG_c")\n\t"
"       fadds  36(%%"REG_c")\n\t"
"       fstps  32(%%"REG_d")\n\t"

"       flds   32(%%"REG_c")\n\t"
"       fsubs  36(%%"REG_c")\n\t"
"       fmuls 120(%%"REG_b")\n\t"
"       fstps  36(%%"REG_d")\n\t"

"       flds   44(%%"REG_c")\n\t"
"       fsubs  40(%%"REG_c")\n\t"
"       fmuls 120(%%"REG_b")\n\t"
"       fsts   44(%%"REG_d")\n\t"
"       fadds  40(%%"REG_c")\n\t"
"       fadds  44(%%"REG_c")\n\t"
"       fstps  40(%%"REG_d")\n\t"

"       flds   48(%%"REG_c")\n\t"
"       fsubs  52(%%"REG_c")\n\t"
"       fmuls 120(%%"REG_b")\n\t"

"       flds   60(%%"REG_c")\n\t"
"       fsubs  56(%%"REG_c")\n\t"
"       fmuls 120(%%"REG_b")\n\t"
"       fld      %%st(0)\n\t"
"       fadds  56(%%"REG_c")\n\t"
"       fadds  60(%%"REG_c")\n\t"
"       fld      %%st(0)\n\t"
"       fadds  48(%%"REG_c")\n\t"
"       fadds  52(%%"REG_c")\n\t"
"       fstps  48(%%"REG_d")\n\t"
"       fadd     %%st(2)\n\t"
"       fstps  56(%%"REG_d")\n\t"
"       fsts   60(%%"REG_d")\n\t"
"       faddp    %%st(1)\n\t"
"       fstps  52(%%"REG_d")\n\t"

"       flds   64(%%"REG_c")\n\t"
"       fadds  68(%%"REG_c")\n\t"
"       fstps  64(%%"REG_d")\n\t"

"       flds   64(%%"REG_c")\n\t"
"       fsubs  68(%%"REG_c")\n\t"
"       fmuls 120(%%"REG_b")\n\t"
"       fstps  68(%%"REG_d")\n\t"

"       flds   76(%%"REG_c")\n\t"
"       fsubs  72(%%"REG_c")\n\t"
"       fmuls 120(%%"REG_b")\n\t"
"       fsts   76(%%"REG_d")\n\t"
"       fadds  72(%%"REG_c")\n\t"
"       fadds  76(%%"REG_c")\n\t"
"       fstps  72(%%"REG_d")\n\t"

"       flds   92(%%"REG_c")\n\t"
"       fsubs  88(%%"REG_c")\n\t"
"       fmuls 120(%%"REG_b")\n\t"
"       fsts   92(%%"REG_d")\n\t"
"       fadds  92(%%"REG_c")\n\t"
"       fadds  88(%%"REG_c")\n\t"
"       fld      %%st(0)\n\t"
"       fadds  80(%%"REG_c")\n\t"
"       fadds  84(%%"REG_c")\n\t"
"       fstps  80(%%"REG_d")\n\t"

"       flds   80(%%"REG_c")\n\t"
"       fsubs  84(%%"REG_c")\n\t"
"       fmuls 120(%%"REG_b")\n\t"
"       fadd  %%st(0), %%st(1)\n\t"
"       fadds 92(%%"REG_d")\n\t"
"       fstps 84(%%"REG_d")\n\t"
"       fstps 88(%%"REG_d")\n\t"

"       flds   96(%%"REG_c")\n\t"
"       fadds 100(%%"REG_c")\n\t"
"       fstps  96(%%"REG_d")\n\t"

"       flds   96(%%"REG_c")\n\t"
"       fsubs 100(%%"REG_c")\n\t"
"       fmuls 120(%%"REG_b")\n\t"
"       fstps 100(%%"REG_d")\n\t"

"       flds  108(%%"REG_c")\n\t"
"       fsubs 104(%%"REG_c")\n\t"
"       fmuls 120(%%"REG_b")\n\t"
"       fsts  108(%%"REG_d")\n\t"
"       fadds 104(%%"REG_c")\n\t"
"       fadds 108(%%"REG_c")\n\t"
"       fstps 104(%%"REG_d")\n\t"

"       flds  124(%%"REG_c")\n\t"
"       fsubs 120(%%"REG_c")\n\t"
"       fmuls 120(%%"REG_b")\n\t"
"       fsts  124(%%"REG_d")\n\t"
"       fadds 120(%%"REG_c")\n\t"
"       fadds 124(%%"REG_c")\n\t"
"       fld      %%st(0)\n\t"
"       fadds 112(%%"REG_c")\n\t"
"       fadds 116(%%"REG_c")\n\t"
"       fstps 112(%%"REG_d")\n\t"

"       flds  112(%%"REG_c")\n\t"
"       fsubs 116(%%"REG_c")\n\t"
"       fmuls 120(%%"REG_b")\n\t"
"       fadd  %%st(0),%%st(1)\n\t"
"       fadds 124(%%"REG_d")\n\t"
"       fstps 116(%%"REG_d")\n\t"
"       fstps 120(%%"REG_d")\n\t"
"       jnz .L01\n\t"

/* Phase 6*/

"       flds      (%%"REG_c")\n\t"
"       fadds    4(%%"REG_c")\n\t"
"       fstps 1024(%%"REG_S")\n\t"

"       flds      (%%"REG_c")\n\t"
"       fsubs    4(%%"REG_c")\n\t"
"       fmuls  120(%%"REG_b")\n\t"
"       fsts      (%%"REG_S")\n\t"
"       fstps     (%%"REG_D")\n\t"

"       flds   12(%%"REG_c")\n\t"
"       fsubs   8(%%"REG_c")\n\t"
"       fmuls 120(%%"REG_b")\n\t"
"       fsts  512(%%"REG_D")\n\t"
"       fadds  12(%%"REG_c")\n\t"
"       fadds   8(%%"REG_c")\n\t"
"       fstps 512(%%"REG_S")\n\t"

"       flds   16(%%"REG_c")\n\t"
"       fsubs  20(%%"REG_c")\n\t"
"       fmuls 120(%%"REG_b")\n\t"

"       flds   28(%%"REG_c")\n\t"
"       fsubs  24(%%"REG_c")\n\t"
"       fmuls 120(%%"REG_b")\n\t"
"       fsts  768(%%"REG_D")\n\t"
"       fld      %%st(0)\n\t"
"       fadds  24(%%"REG_c")\n\t"
"       fadds  28(%%"REG_c")\n\t"
"       fld      %%st(0)\n\t"
"       fadds  16(%%"REG_c")\n\t"
"       fadds  20(%%"REG_c")\n\t"
"       fstps 768(%%"REG_S")\n\t"
"       fadd     %%st(2)\n\t"
"       fstps 256(%%"REG_S")\n\t"
"       faddp    %%st(1)\n\t"
"       fstps 256(%%"REG_D")\n\t"

/* Phase 7*/

"       flds   32(%%"REG_d")\n\t"
"       fadds  48(%%"REG_d")\n\t"
"       fstps 896(%%"REG_S")\n\t"

"       flds   48(%%"REG_d")\n\t"
"       fadds  40(%%"REG_d")\n\t"
"       fstps 640(%%"REG_S")\n\t"

"       flds   40(%%"REG_d")\n\t"
"       fadds  56(%%"REG_d")\n\t"
"       fstps 384(%%"REG_S")\n\t"

"       flds   56(%%"REG_d")\n\t"
"       fadds  36(%%"REG_d")\n\t"
"       fstps 128(%%"REG_S")\n\t"

"       flds   36(%%"REG_d")\n\t"
"       fadds  52(%%"REG_d")\n\t"
"       fstps 128(%%"REG_D")\n\t"

"       flds   52(%%"REG_d")\n\t"
"       fadds  44(%%"REG_d")\n\t"
"       fstps 384(%%"REG_D")\n\t"

"       flds   60(%%"REG_d")\n\t"
"       fsts  896(%%"REG_D")\n\t"
"       fadds  44(%%"REG_d")\n\t"
"       fstps 640(%%"REG_D")\n\t"

"       flds   96(%%"REG_d")\n\t"
"       fadds 112(%%"REG_d")\n\t"
"       fld      %%st(0)\n\t"
"       fadds  64(%%"REG_d")\n\t"
"       fstps 960(%%"REG_S")\n\t"
"       fadds  80(%%"REG_d")\n\t"
"       fstps 832(%%"REG_S")\n\t"

"       flds  112(%%"REG_d")\n\t"
"       fadds 104(%%"REG_d")\n\t"
"       fld      %%st(0)\n\t"
"       fadds  80(%%"REG_d")\n\t"
"       fstps 704(%%"REG_S")\n\t"
"       fadds  72(%%"REG_d")\n\t"
"       fstps 576(%%"REG_S")\n\t"

"       flds  104(%%"REG_d")\n\t"
"       fadds 120(%%"REG_d")\n\t"
"       fld      %%st(0)\n\t"
"       fadds  72(%%"REG_d")\n\t"
"       fstps 448(%%"REG_S")\n\t"
"       fadds  88(%%"REG_d")\n\t"
"       fstps 320(%%"REG_S")\n\t"

"       flds  120(%%"REG_d")\n\t"
"       fadds 100(%%"REG_d")\n\t"
"       fld      %%st(0)\n\t"
"       fadds  88(%%"REG_d")\n\t"
"       fstps 192(%%"REG_S")\n\t"
"       fadds  68(%%"REG_d")\n\t"
"       fstps  64(%%"REG_S")\n\t"

"       flds  100(%%"REG_d")\n\t"
"       fadds 116(%%"REG_d")\n\t"
"       fld      %%st(0)\n\t"
"       fadds  68(%%"REG_d")\n\t"
"       fstps  64(%%"REG_D")\n\t"
"       fadds  84(%%"REG_d")\n\t"
"       fstps 192(%%"REG_D")\n\t"

"       flds  116(%%"REG_d")\n\t"
"       fadds 108(%%"REG_d")\n\t"
"       fld      %%st(0)\n\t"
"       fadds  84(%%"REG_d")\n\t"
"       fstps 320(%%"REG_D")\n\t"
"       fadds  76(%%"REG_d")\n\t"
"       fstps 448(%%"REG_D")\n\t"

"       flds  108(%%"REG_d")\n\t"
"       fadds 124(%%"REG_d")\n\t"
"       fld      %%st(0)\n\t"
"       fadds  76(%%"REG_d")\n\t"
"       fstps 576(%%"REG_D")\n\t"
"       fadds  92(%%"REG_d")\n\t"
"       fstps 704(%%"REG_D")\n\t"

"       flds  124(%%"REG_d")\n\t"
"       fsts  960(%%"REG_D")\n\t"
"       fadds  92(%%"REG_d")\n\t"
"       fstps 832(%%"REG_D")\n\t"
"       jmp     .L_bye\n\t"
".L01:\n\t"
/* Phase 8*/

"       flds      (%%"REG_c")\n\t"
"       fadds    4(%%"REG_c")\n\t"
"       fistp  512(%%"REG_S")\n\t"

"       flds      (%%"REG_c")\n\t"
"       fsubs    4(%%"REG_c")\n\t"
"       fmuls  120(%%"REG_b")\n\t"

"       fistp     (%%"REG_S")\n\t"


"       flds    12(%%"REG_c")\n\t"
"       fsubs    8(%%"REG_c")\n\t"
"       fmuls  120(%%"REG_b")\n\t"
"       fist   256(%%"REG_D")\n\t"
"       fadds   12(%%"REG_c")\n\t"
"       fadds    8(%%"REG_c")\n\t"
"       fistp  256(%%"REG_S")\n\t"

"       flds   16(%%"REG_c")\n\t"
"       fsubs  20(%%"REG_c")\n\t"
"       fmuls 120(%%"REG_b")\n\t"

"       flds   28(%%"REG_c")\n\t"
"       fsubs  24(%%"REG_c")\n\t"
"       fmuls 120(%%"REG_b")\n\t"
"       fist  384(%%"REG_D")\n\t"
"       fld      %%st(0)\n\t"
"       fadds  24(%%"REG_c")\n\t"
"       fadds  28(%%"REG_c")\n\t"
"       fld      %%st(0)\n\t"
"       fadds  16(%%"REG_c")\n\t"
"       fadds  20(%%"REG_c")\n\t"
"       fistp  384(%%"REG_S")\n\t"
"       fadd     %%st(2)\n\t"
"       fistp  128(%%"REG_S")\n\t"
"       faddp    %%st(1)\n\t"
"       fistp  128(%%"REG_D")\n\t"

/* Phase 9*/

"       flds    32(%%"REG_d")\n\t"
"       fadds   48(%%"REG_d")\n\t"
"       fistp  448(%%"REG_S")\n\t"

"       flds   48(%%"REG_d")\n\t"
"       fadds  40(%%"REG_d")\n\t"
"       fistp 320(%%"REG_S")\n\t"

"       flds   40(%%"REG_d")\n\t"
"       fadds  56(%%"REG_d")\n\t"
"       fistp 192(%%"REG_S")\n\t"

"       flds   56(%%"REG_d")\n\t"
"       fadds  36(%%"REG_d")\n\t"
"       fistp  64(%%"REG_S")\n\t"

"       flds   36(%%"REG_d")\n\t"
"       fadds  52(%%"REG_d")\n\t"
"       fistp  64(%%"REG_D")\n\t"

"       flds   52(%%"REG_d")\n\t"
"       fadds  44(%%"REG_d")\n\t"
"       fistp 192(%%"REG_D")\n\t"

"       flds   60(%%"REG_d")\n\t"
"       fist   448(%%"REG_D")\n\t"
"       fadds  44(%%"REG_d")\n\t"
"       fistp 320(%%"REG_D")\n\t"

"       flds   96(%%"REG_d")\n\t"
"       fadds 112(%%"REG_d")\n\t"
"       fld      %%st(0)\n\t"
"       fadds  64(%%"REG_d")\n\t"
"       fistp 480(%%"REG_S")\n\t"
"       fadds  80(%%"REG_d")\n\t"
"       fistp 416(%%"REG_S")\n\t"

"       flds  112(%%"REG_d")\n\t"
"       fadds 104(%%"REG_d")\n\t"
"       fld      %%st(0)\n\t"
"       fadds  80(%%"REG_d")\n\t"
"       fistp 352(%%"REG_S")\n\t"
"       fadds  72(%%"REG_d")\n\t"
"       fistp 288(%%"REG_S")\n\t"

"       flds  104(%%"REG_d")\n\t"
"       fadds 120(%%"REG_d")\n\t"
"       fld      %%st(0)\n\t"
"       fadds  72(%%"REG_d")\n\t"
"       fistp 224(%%"REG_S")\n\t"
"       fadds  88(%%"REG_d")\n\t"
"       fistp 160(%%"REG_S")\n\t"

"       flds  120(%%"REG_d")\n\t"
"       fadds 100(%%"REG_d")\n\t"
"       fld      %%st(0)\n\t"
"       fadds  88(%%"REG_d")\n\t"
"       fistp  96(%%"REG_S")\n\t"
"       fadds  68(%%"REG_d")\n\t"
"       fistp  32(%%"REG_S")\n\t"

"       flds  100(%%"REG_d")\n\t"
"       fadds 116(%%"REG_d")\n\t"
"       fld      %%st(0)\n\t"
"       fadds  68(%%"REG_d")\n\t"
"       fistp  32(%%"REG_D")\n\t"
"       fadds  84(%%"REG_d")\n\t"
"       fistp  96(%%"REG_D")\n\t"

"       flds  116(%%"REG_d")\n\t"
"       fadds 108(%%"REG_d")\n\t"
"       fld      %%st(0)\n\t"
"       fadds  84(%%"REG_d")\n\t"
"       fistp 160(%%"REG_D")\n\t"
"       fadds  76(%%"REG_d")\n\t"
"       fistp 224(%%"REG_D")\n\t"

"       flds  108(%%"REG_d")\n\t"
"       fadds 124(%%"REG_d")\n\t"
"       fld      %%st(0)\n\t"
"       fadds  76(%%"REG_d")\n\t"
"       fistp 288(%%"REG_D")\n\t"
"       fadds  92(%%"REG_d")\n\t"
"       fistp 352(%%"REG_D")\n\t"

"       flds  124(%%"REG_d")\n\t"
"       fist  480(%%"REG_D")\n\t"
"       fadds  92(%%"REG_d")\n\t"
"       fistp 416(%%"REG_D")\n\t"
"       movsw\n\t"
".L_bye:"
        :
        :"m"(a),"m"(b),"m"(c),"m"(tmp[0])
        :"memory","%eax","%ebx","%ecx","%edx","%esi","%edi");
}
