/*
 * This code was taken from http://www.mpg123.org
 * See ChangeLog of mpg123-0.59s-pre.1 for detail
 * Applied to mplayer by Nick Kurshev <nickols_k@mail.ru>
*/
#include "config.h"
#include "mangle.h"

long __attribute__((aligned(8))) mp3lib_decwins [544];

#define real float
extern real mp3lib_decwin[(512+32)];
// static long decwin [544];

static short attribute_used intwinbase_MMX[] = 
{
	      0,    -1,    -1,    -1,    -1,    -1,    -1,    -2,
	     -2,    -2,    -2,    -3,    -3,    -4,    -4,    -5,
	     -5,    -6,    -7,    -7,    -8,    -9,   -10,   -11,
	    -13,   -14,   -16,   -17,   -19,   -21,   -24,   -26,
	    -29,   -31,   -35,   -38,   -41,   -45,   -49,   -53,
	    -58,   -63,   -68,   -73,   -79,   -85,   -91,   -97,
	   -104,  -111,  -117,  -125,  -132,  -139,  -147,  -154,
	   -161,  -169,  -176,  -183,  -190,  -196,  -202,  -208,
	   -213,  -218,  -222,  -225,  -227,  -228,  -228,  -227,
	   -224,  -221,  -215,  -208,  -200,  -189,  -177,  -163,
	   -146,  -127,  -106,   -83,   -57,   -29,     2,    36,
	     72,   111,   153,   197,   244,   294,   347,   401,
	    459,   519,   581,   645,   711,   779,   848,   919,
	    991,  1064,  1137,  1210,  1283,  1356,  1428,  1498,
	   1567,  1634,  1698,  1759,  1817,  1870,  1919,  1962,
	   2001,  2032,  2057,  2075,  2085,  2087,  2080,  2063,
	   2037,  2000,  1952,  1893,  1822,  1739,  1644,  1535,
	   1414,  1280,  1131,   970,   794,   605,   402,   185,
	    -45,  -288,  -545,  -814, -1095, -1388, -1692, -2006,
	  -2330, -2663, -3004, -3351, -3705, -4063, -4425, -4788,
	  -5153, -5517, -5879, -6237, -6589, -6935, -7271, -7597,
	  -7910, -8209, -8491, -8755, -8998, -9219, -9416, -9585,
	  -9727, -9838, -9916, -9959, -9966, -9935, -9863, -9750,
	  -9592, -9389, -9139, -8840, -8492, -8092, -7640, -7134,
	  -6574, -5959, -5288, -4561, -3776, -2935, -2037, -1082,
	    -70,   998,  2122,  3300,  4533,  5818,  7154,  8540,
	   9975, 11455, 12980, 14548, 16155, 17799, 19478, 21189,
	  22929, 24694, 26482, 28289, 30112, 31947,-26209,-24360,
	 -22511,-20664,-18824,-16994,-15179,-13383,-11610, -9863,
	  -8147, -6466, -4822, -3222, -1667,  -162,  1289,  2684,
	   4019,  5290,  6494,  7629,  8692,  9679, 10590, 11420,
	  12169, 12835, 13415, 13908, 14313, 14630, 14856, 14992,
	  15038
};

static long attribute_used intwindiv = 0x47800000;

void make_decode_tables_MMX(long scaleval)
{
  long intwinbase_step;
  intwinbase_step=2;
  scaleval =- scaleval;
    __asm __volatile(
	"xorl %%ecx,%%ecx\n\t"
	"xorl %%ebx,%%ebx\n\t"
	"movl $32,%%esi\n\t"
	"movl %0,%%edi\n\t"
".L00:\n\t"
	"cmpl $528,%%ecx\n\t"
	"jnc .L02\n\t"
	"movswl (%%edi),%%eax\n\t"
	"cmpl %0+444,%%edi\n\t"
	"jc .L01\n\t"
	"addl $60000,%%eax\n\t"
".L01:\n\t"
	"pushl %%eax\n\t"
	"fildl (%%esp)\n\t"
	"fdivs "MANGLE(intwindiv)"\n\t"
	"popl %%eax\n\t"
	"fimull %1\n\t"
	"fsts  "MANGLE(mp3lib_decwin)"(,%%ecx,4)\n\t"
	"fstps "MANGLE(mp3lib_decwin)"+64(,%%ecx,4)\n\t"
".L02:\n\t"
	"leal -1(%%esi),%%edx\n\t"
	"and %%ebx,%%edx\n\t"
	"cmp $31,%%edx\n\t"
	"jnz .L03\n\t"
	"addl $-1023,%%ecx\n\t"
	"test %%esi,%%ebx\n\t"
	"jz  .L03\n\t"
	"negl %1\n\t"
".L03:\n\t"
	"addl %%esi,%%ecx\n\t"
	"addl %2,%%edi\n\t"
	"incl %%ebx\n\t"
	"cmpl %0,%%edi\n\t"
	"jz .L04\n\t"
	"cmp $256,%%ebx\n\t"
	"jnz .L00\n\t"
	"negl %2\n\t"
	"jmp .L00\n\t"
".L04:\n\t"
	::"g"(intwinbase_MMX),"m"(scaleval),"m"(intwinbase_step)
	:"memory","%eax","%ebx","%ecx","%edx","%esi","%edi");
intwinbase_step=2;
  __asm __volatile(
	"xorl %%ecx,%%ecx\n\t"
	"xorl %%ebx,%%ebx\n\t"
".L05:\n\t"
	"cmpl $528,%%ecx\n\t"
	"jnc .L11\n\t"
	"movswl (%%edi),%%eax\n\t"
	"cmpl %0+444,%%edi\n\t"
	"jc .L06\n\t"
	"addl $60000,%%eax\n\t"
".L06:\n\t"
	"cltd\n\t"
	"imull %1\n\t"
	"shrdl $17,%%edx,%%eax\n\t"
	"cmpl $32767,%%eax\n\t"
	"movl $1055,%%edx\n\t"
	"jle .L07\n\t"
	"movl $32767,%%eax\n\t"
	"jmp .L08\n\t"
".L07:\n\t"
	"cmpl $-32767,%%eax\n\t"
	"jge .L08\n\t"
	"movl $-32767,%%eax\n\t"
".L08:\n\t"
	"cmpl $512,%%ecx\n\t"
	"jnc .L09\n\t"
	"subl %%ecx,%%edx\n\t"
	"movw %%ax,"MANGLE(mp3lib_decwins)"(,%%edx,2)\n\t"
	"movw %%ax,"MANGLE(mp3lib_decwins)"-32(,%%edx,2)\n\t"
".L09:\n\t"
	"testl $1,%%ecx\n\t"
	"jnz .L10\n\t"
	"negl %%eax\n\t"
".L10:\n\t"
	"movw %%ax,"MANGLE(mp3lib_decwins)"(,%%ecx,2)\n\t"
	"movw %%ax,"MANGLE(mp3lib_decwins)"+32(,%%ecx,2)\n\t"
".L11:\n\t"
	"leal -1(%%esi),%%edx\n\t"
	"and %%ebx,%%edx\n\t"
	"cmp $31,%%edx\n\t"
	"jnz .L12\n\t"
	"addl $-1023,%%ecx\n\t"
	"test %%esi,%%ebx\n\t"
	"jz  .L12\n\t"
	"negl %1\n\t"
".L12:\n\t"
	"addl %%esi,%%ecx\n\t"
	"addl %2,%%edi\n\t"
	"incl %%ebx\n\t"
	"cmpl %0,%%edi\n\t"
	"jz .L13\n\t"
	"cmp $256,%%ebx\n\t"
	"jnz .L05\n\t"
	"negl %2\n\t"
	"jmp .L05\n\t"
".L13:\n\t"
	::"g"(intwinbase_MMX),"m"(scaleval),"m"(intwinbase_step)
	:"memory","%eax","%ebx","%ecx","%edx","%esi","%edi");
}
