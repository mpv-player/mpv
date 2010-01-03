/*
 * aclib - advanced C library ;)
 * Functions which improve and expand the standard C library, see aclib_template.c.
 * This file only contains runtime CPU detection and config option stuff.
 * runtime CPU detection by Michael Niedermayer (michaelni@gmx.at)
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "cpudetect.h"
#include "fastmemcpy.h"
#include "libavutil/x86_cpu.h"
#undef memcpy

#define BLOCK_SIZE 4096
#define CONFUSION_FACTOR 0
//Feel free to fine-tune the above 2, it might be possible to get some speedup with them :)

//#define STATISTICS

//Note: we have MMX, MMX2, 3DNOW version there is no 3DNOW+MMX2 one
//Plain C versions
//#if !HAVE_MMX || CONFIG_RUNTIME_CPUDETECT
//#define COMPILE_C
//#endif

#if ARCH_X86

#if (HAVE_MMX && !HAVE_AMD3DNOW && !HAVE_MMX2) || CONFIG_RUNTIME_CPUDETECT
#define COMPILE_MMX
#endif

#if (HAVE_MMX2 && !HAVE_SSE2) || CONFIG_RUNTIME_CPUDETECT
#define COMPILE_MMX2
#endif

#if (HAVE_AMD3DNOW && !HAVE_MMX2) || CONFIG_RUNTIME_CPUDETECT
#define COMPILE_3DNOW
#endif

#if HAVE_SSE2 || CONFIG_RUNTIME_CPUDETECT
#define COMPILE_SSE
#endif

#undef HAVE_MMX
#undef HAVE_MMX2
#undef HAVE_AMD3DNOW
#undef HAVE_SSE
#undef HAVE_SSE2
#define HAVE_MMX 0
#define HAVE_MMX2 0
#define HAVE_AMD3DNOW 0
#define HAVE_SSE 0
#define HAVE_SSE2 0
/*
#ifdef COMPILE_C
#undef HAVE_MMX
#undef HAVE_MMX2
#undef HAVE_AMD3DNOW
#undef HAVE_SSE
#undef HAVE_SSE2
#define HAVE_MMX 0
#define HAVE_MMX2 0
#define HAVE_AMD3DNOW 0
#define HAVE_SSE 0
#define HAVE_SSE2 0
#define RENAME(a) a ## _C
#include "aclib_template.c"
#endif
*/
//MMX versions
#ifdef COMPILE_MMX
#undef RENAME
#undef HAVE_MMX
#undef HAVE_MMX2
#undef HAVE_AMD3DNOW
#undef HAVE_SSE
#undef HAVE_SSE2
#define HAVE_MMX 1
#define HAVE_MMX2 0
#define HAVE_AMD3DNOW 0
#define HAVE_SSE 0
#define HAVE_SSE2 0
#define RENAME(a) a ## _MMX
#include "aclib_template.c"
#endif

//MMX2 versions
#ifdef COMPILE_MMX2
#undef RENAME
#undef HAVE_MMX
#undef HAVE_MMX2
#undef HAVE_AMD3DNOW
#undef HAVE_SSE
#undef HAVE_SSE2
#define HAVE_MMX 1
#define HAVE_MMX2 1
#define HAVE_AMD3DNOW 0
#define HAVE_SSE 0
#define HAVE_SSE2 0
#define RENAME(a) a ## _MMX2
#include "aclib_template.c"
#endif

//3DNOW versions
#ifdef COMPILE_3DNOW
#undef RENAME
#undef HAVE_MMX
#undef HAVE_MMX2
#undef HAVE_AMD3DNOW
#undef HAVE_SSE
#undef HAVE_SSE2
#define HAVE_MMX 1
#define HAVE_MMX2 0
#define HAVE_AMD3DNOW 1
#define HAVE_SSE 0
#define HAVE_SSE2 0
#define RENAME(a) a ## _3DNow
#include "aclib_template.c"
#endif

//SSE versions (only used on SSE2 cpus)
#ifdef COMPILE_SSE
#undef RENAME
#undef HAVE_MMX
#undef HAVE_MMX2
#undef HAVE_AMD3DNOW
#undef HAVE_SSE
#undef HAVE_SSE2
#define HAVE_MMX 1
#define HAVE_MMX2 1
#define HAVE_AMD3DNOW 0
#define HAVE_SSE 1
#define HAVE_SSE2 1
#define RENAME(a) a ## _SSE
#include "aclib_template.c"
#endif

#endif /* ARCH_X86 */


#undef fast_memcpy
void * fast_memcpy(void * to, const void * from, size_t len)
{
#if CONFIG_RUNTIME_CPUDETECT
#if ARCH_X86
	// ordered per speed fasterst first
	if(gCpuCaps.hasSSE2)
		fast_memcpy_SSE(to, from, len);
	else if(gCpuCaps.hasMMX2)
		fast_memcpy_MMX2(to, from, len);
	else if(gCpuCaps.has3DNow)
		fast_memcpy_3DNow(to, from, len);
	else if(gCpuCaps.hasMMX)
		fast_memcpy_MMX(to, from, len);
	else
#endif
		memcpy(to, from, len); // prior to mmx we use the standart memcpy
#else
#if HAVE_SSE2
		fast_memcpy_SSE(to, from, len);
#elif HAVE_MMX2
		fast_memcpy_MMX2(to, from, len);
#elif HAVE_AMD3DNOW
		fast_memcpy_3DNow(to, from, len);
#elif HAVE_MMX
		fast_memcpy_MMX(to, from, len);
#else
		memcpy(to, from, len); // prior to mmx we use the standart memcpy
#endif

#endif //!CONFIG_RUNTIME_CPUDETECT
	return to;
}

#undef	mem2agpcpy
void * mem2agpcpy(void * to, const void * from, size_t len)
{
#if CONFIG_RUNTIME_CPUDETECT
#if ARCH_X86
	// ordered per speed fasterst first
	if(gCpuCaps.hasSSE2)
		mem2agpcpy_SSE(to, from, len);
	else if(gCpuCaps.hasMMX2)
		mem2agpcpy_MMX2(to, from, len);
	else if(gCpuCaps.has3DNow)
		mem2agpcpy_3DNow(to, from, len);
	else if(gCpuCaps.hasMMX)
		mem2agpcpy_MMX(to, from, len);
	else
#endif
		memcpy(to, from, len); // prior to mmx we use the standart memcpy
#else
#if HAVE_SSE2
		mem2agpcpy_SSE(to, from, len);
#elif HAVE_MMX2
		mem2agpcpy_MMX2(to, from, len);
#elif HAVE_AMD3DNOW
		mem2agpcpy_3DNow(to, from, len);
#elif HAVE_MMX
		mem2agpcpy_MMX(to, from, len);
#else
		memcpy(to, from, len); // prior to mmx we use the standart memcpy
#endif

#endif //!CONFIG_RUNTIME_CPUDETECT
	return to;
}
