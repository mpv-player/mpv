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

static int temp; // buggy gcc 3.x fails if this is moved into the function :(
void synth_1to1_MMX_s(real *bandPtr, int channel, short *samples,
                      short *buffs, int *bo)
{

__asm __volatile(
        "movl %1,%%ecx\n\t"
        "movl %2,%%edi\n\t"
        "movl $15,%%ebx\n\t"
        "movl %4,%%edx\n\t"
        "leal (%%edi,%%ecx,2),%%edi\n\t"
	"decl %%ecx\n\t"
        "movl %3,%%esi\n\t"
        "movl (%%edx),%%eax\n\t"
        "jecxz .L01\n\t"
        "decl %%eax\n\t"
        "andl %%ebx,%%eax\n\t"
        "leal 1088(%%esi),%%esi\n\t"
        "movl %%eax,(%%edx)\n\t"
".L01:\n\t"
        "leal (%%esi,%%eax,2),%%edx\n\t"
        "movl %%eax,%5\n\t"
        "incl %%eax\n\t"
        "andl %%ebx,%%eax\n\t"
        "leal 544(%%esi,%%eax,2),%%ecx\n\t"
	"incl %%ebx\n\t"
	"testl $1, %%eax\n\t"
	"jnz .L02\n\t"
        "xchgl %%edx,%%ecx\n\t"
	"incl %5\n\t"
        "leal 544(%%esi),%%esi\n\t"
".L02:\n\t"
	"emms\n\t"
        "pushl %0\n\t"
        "pushl %%edx\n\t"
        "pushl %%ecx\n\t"
        "call *"MANGLE(dct64_MMX_func)"\n\t"
	"addl $12, %%esp\n\t"
	"leal 1(%%ebx), %%ecx\n\t"
        "subl %5,%%ebx\n\t"
	"pushl %%ecx\n\t"
	"leal "MANGLE(mp3lib_decwins)"(%%ebx,%%ebx,1), %%edx\n\t"
	"shrl $1, %%ecx\n\t"
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

        "leal 64(%%esi),%%esi\n\t"
        "leal 128(%%edx),%%edx\n\t"
        "leal 8(%%edi),%%edi\n\t"

	"decl %%ecx\n\t"
        "jnz  .L03\n\t"

	"popl %%ecx\n\t"
	"andl $1, %%ecx\n\t"
	"jecxz .next_loop\n\t"

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
        "leal 32(%%esi),%%esi\n\t"
        "leal 64(%%edx),%%edx\n\t"
        "leal 4(%%edi),%%edi\n\t"               
	
".next_loop:\n\t"
        "subl $64,%%esi\n\t"
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

        "subl $64,%%esi\n\t"
        "addl $128,%%edx\n\t"
        "leal 8(%%edi),%%edi\n\t"
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
        :
	:"m"(bandPtr),"m"(channel),"m"(samples),"m"(buffs),"m"(bo), "m"(temp)
	:"memory","%edi","%esi","%eax","%ebx","%ecx","%edx","%esp");
}
