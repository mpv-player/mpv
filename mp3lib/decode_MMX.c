/*
 * this code comes under GPL
 * This code was taken from http://www.mpg123.org
 * See ChangeLog of mpg123-0.59s-pre.1 for detail
 * Applied to mplayer by Nick Kurshev <nickols_k@mail.ru>
 *
 * Local ChangeLog:
 * - Partial loops unrolling and removing MOVW insn from loops
*/
#include "config.h"
#include "mangle.h"
#define real float /* ugly - but only way */

extern short mp3lib_decwins[];
extern void (*dct64_MMX_func)(short*, short*, real*);
static unsigned long long attribute_used __attribute__((aligned(8))) null_one = 0x0000ffff0000ffffULL;
static unsigned long long attribute_used __attribute__((aligned(8))) one_null = 0xffff0000ffff0000ULL;
unsigned long __attribute__((aligned(16))) costab_mmx[] =
{
	1056974725,
	1057056395,
	1057223771,
	1057485416,
	1057855544,
	1058356026,
	1059019886,
	1059897405,
	1061067246,
	1062657950,
	1064892987,
	1066774581,
	1069414683,
	1073984175,
	1079645762,
	1092815430,
	1057005197,
	1057342072,
	1058087743,
	1059427869,
	1061799040,
	1065862217,
	1071413542,
	1084439708,
	1057128951,
	1058664893,
	1063675095,
	1076102863,
	1057655764,
	1067924853,
	1060439283,
};

int synth_1to1_MMX(real *bandPtr, int channel, short *samples)
{
    static short buffs[2][2][0x110] __attribute__((aligned(8)));
    static int bo = 1;
    short *b0, (*buf)[0x110], *a, *b;
    short* window;
    int bo1, i = 8;

    if (channel == 0) {
	bo = (bo - 1) & 0xf;
	buf = buffs[1];
    } else {
	samples++;
	buf = buffs[0];
    }

    if (bo & 1) {
	b0 = buf[1];
	bo1 = bo + 1;
       	a = buf[0] + bo;
	b = buf[1] + ((bo + 1) & 0xf);
    } else {
	b0 = buf[0];
	bo1 = bo;
	b = buf[0] + bo;
       	a = buf[1] + ((bo + 1) & 0xf);
    }

    dct64_MMX_func(a, b, bandPtr);
    window = mp3lib_decwins + 16 - bo1;
    //printf("DEBUG: channel %d, bo %d, off %d\n", channel, bo, 16 - bo1);
__asm __volatile(
ASMALIGN(4)
".L03:\n\t"
        "movq  (%%edx),%%mm0\n\t"
        "movq  64(%%edx),%%mm4\n\t"
        "pmaddwd (%%esi),%%mm0\n\t"
        "pmaddwd 32(%%esi),%%mm4\n\t"
        "movq  8(%%edx),%%mm1\n\t"
        "movq  72(%%edx),%%mm5\n\t"
        "pmaddwd 8(%%esi),%%mm1\n\t"
        "pmaddwd 40(%%esi),%%mm5\n\t"
        "movq  16(%%edx),%%mm2\n\t"
        "movq  80(%%edx),%%mm6\n\t"
        "pmaddwd 16(%%esi),%%mm2\n\t"
        "pmaddwd 48(%%esi),%%mm6\n\t"
        "movq  24(%%edx),%%mm3\n\t"
        "movq  88(%%edx),%%mm7\n\t"
        "pmaddwd 24(%%esi),%%mm3\n\t"
        "pmaddwd 56(%%esi),%%mm7\n\t"
        "paddd %%mm1,%%mm0\n\t"
        "paddd %%mm5,%%mm4\n\t"
        "paddd %%mm2,%%mm0\n\t"
        "paddd %%mm6,%%mm4\n\t"
        "paddd %%mm3,%%mm0\n\t"
        "paddd %%mm7,%%mm4\n\t"
        "movq  %%mm0,%%mm1\n\t"
        "movq  %%mm4,%%mm5\n\t"
        "psrlq $32,%%mm1\n\t"
        "psrlq $32,%%mm5\n\t"
        "paddd %%mm1,%%mm0\n\t"
        "paddd %%mm5,%%mm4\n\t"
        "psrad $13,%%mm0\n\t"
        "psrad $13,%%mm4\n\t"
        "packssdw %%mm0,%%mm0\n\t"
        "packssdw %%mm4,%%mm4\n\t"

	"movq	(%%edi), %%mm1\n\t"
	"punpckldq %%mm4, %%mm0\n\t"
	"pand   "MANGLE(one_null)", %%mm1\n\t"
	"pand   "MANGLE(null_one)", %%mm0\n\t"
	"por    %%mm0, %%mm1\n\t"
	"movq   %%mm1,(%%edi)\n\t"

        "add $64,%%esi\n\t"
        "add $128,%%edx\n\t"
        "add $8,%%edi\n\t"

	"decl %%ecx\n\t"
        "jnz  .L03\n\t"

        "movq  (%%edx),%%mm0\n\t"
        "pmaddwd (%%esi),%%mm0\n\t"
        "movq  8(%%edx),%%mm1\n\t"
        "pmaddwd 8(%%esi),%%mm1\n\t"
        "movq  16(%%edx),%%mm2\n\t"
        "pmaddwd 16(%%esi),%%mm2\n\t"
        "movq  24(%%edx),%%mm3\n\t"
        "pmaddwd 24(%%esi),%%mm3\n\t"
        "paddd %%mm1,%%mm0\n\t"
        "paddd %%mm2,%%mm0\n\t"
        "paddd %%mm3,%%mm0\n\t"
        "movq  %%mm0,%%mm1\n\t"
        "psrlq $32,%%mm1\n\t"
        "paddd %%mm1,%%mm0\n\t"
        "psrad $13,%%mm0\n\t"
        "packssdw %%mm0,%%mm0\n\t"
        "movd %%mm0,%%eax\n\t"
	"movw %%ax, (%%edi)\n\t"
        "sub $32,%%esi\n\t"
        "add $64,%%edx\n\t"
        "add $4,%%edi\n\t"               

        "movl $7,%%ecx\n\t"
ASMALIGN(4)
".L04:\n\t"
        "movq  (%%edx),%%mm0\n\t"
        "movq  64(%%edx),%%mm4\n\t"
        "pmaddwd (%%esi),%%mm0\n\t"
        "pmaddwd -32(%%esi),%%mm4\n\t"
        "movq  8(%%edx),%%mm1\n\t"
        "movq  72(%%edx),%%mm5\n\t"
        "pmaddwd 8(%%esi),%%mm1\n\t"
        "pmaddwd -24(%%esi),%%mm5\n\t"
        "movq  16(%%edx),%%mm2\n\t"
        "movq  80(%%edx),%%mm6\n\t"
        "pmaddwd 16(%%esi),%%mm2\n\t"
        "pmaddwd -16(%%esi),%%mm6\n\t"
        "movq  24(%%edx),%%mm3\n\t"
        "movq  88(%%edx),%%mm7\n\t"
        "pmaddwd 24(%%esi),%%mm3\n\t"
        "pmaddwd -8(%%esi),%%mm7\n\t"
        "paddd %%mm1,%%mm0\n\t"
        "paddd %%mm5,%%mm4\n\t"
        "paddd %%mm2,%%mm0\n\t"
        "paddd %%mm6,%%mm4\n\t"
        "paddd %%mm3,%%mm0\n\t"
        "paddd %%mm7,%%mm4\n\t"
        "movq  %%mm0,%%mm1\n\t"
        "movq  %%mm4,%%mm5\n\t"
        "psrlq $32,%%mm1\n\t"
        "psrlq $32,%%mm5\n\t"
        "paddd %%mm0,%%mm1\n\t"
        "paddd %%mm4,%%mm5\n\t"
        "psrad $13,%%mm1\n\t"
        "psrad $13,%%mm5\n\t"
        "packssdw %%mm1,%%mm1\n\t"
        "packssdw %%mm5,%%mm5\n\t"
        "psubd %%mm0,%%mm0\n\t"
        "psubd %%mm4,%%mm4\n\t"
        "psubsw %%mm1,%%mm0\n\t"
        "psubsw %%mm5,%%mm4\n\t"

	"movq	(%%edi), %%mm1\n\t"
	"punpckldq %%mm4, %%mm0\n\t"
	"pand   "MANGLE(one_null)", %%mm1\n\t"
	"pand   "MANGLE(null_one)", %%mm0\n\t"
	"por    %%mm0, %%mm1\n\t"
	"movq   %%mm1,(%%edi)\n\t"

        "sub $64,%%esi\n\t"
        "add $128,%%edx\n\t"
        "add $8,%%edi\n\t"
        "decl %%ecx\n\t"
	"jnz  .L04\n\t"

        "movq  (%%edx),%%mm0\n\t"
        "pmaddwd (%%esi),%%mm0\n\t"
        "movq  8(%%edx),%%mm1\n\t"
        "pmaddwd 8(%%esi),%%mm1\n\t"
        "movq  16(%%edx),%%mm2\n\t"
        "pmaddwd 16(%%esi),%%mm2\n\t"
        "movq  24(%%edx),%%mm3\n\t"
        "pmaddwd 24(%%esi),%%mm3\n\t"
        "paddd %%mm1,%%mm0\n\t"
        "paddd %%mm2,%%mm0\n\t"
        "paddd %%mm3,%%mm0\n\t"
        "movq  %%mm0,%%mm1\n\t"
        "psrlq $32,%%mm1\n\t"
        "paddd %%mm0,%%mm1\n\t"
        "psrad $13,%%mm1\n\t"
        "packssdw %%mm1,%%mm1\n\t"
        "psubd %%mm0,%%mm0\n\t"
        "psubsw %%mm1,%%mm0\n\t"
        "movd %%mm0,%%eax\n\t"
	"movw %%ax,(%%edi)\n\t"
	"emms\n\t"
	:"+c"(i), "+d"(window), "+S"(b0), "+D"(samples)
	:
	:"memory", "%eax");
    return 0;
}

