/*
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
#include "cpudetect.h"
#include "mp_msg.h"

CpuCaps gCpuCaps;

#include <stdlib.h>

#if ARCH_X86

#include <stdio.h>
#include <string.h>

#if defined (__NetBSD__) || defined(__OpenBSD__)
#include <sys/param.h>
#include <sys/sysctl.h>
#include <machine/cpu.h>
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__) || defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#elif defined(__linux__)
#include <signal.h>
#elif defined(__MINGW32__) || defined(__CYGWIN__)
#include <windows.h>
#elif defined(__OS2__)
#define INCL_DOS
#include <os2.h>
#elif defined(__AMIGAOS4__)
#include <proto/exec.h>
#endif

/* Thanks to the FreeBSD project for some of this cpuid code, and
 * help understanding how to use it.  Thanks to the Mesa
 * team for SSE support detection and more cpu detect code.
 */

/* I believe this code works.  However, it has only been used on a PII and PIII */

static void check_os_katmai_support( void );

// return TRUE if cpuid supported
static int has_cpuid(void)
{
// code from libavcodec:
#if ARCH_X86_64
   return 1;
#else
   long a, c;
   __asm__ volatile (
       /* See if CPUID instruction is supported ... */
       /* ... Get copies of EFLAGS into eax and ecx */
       "pushfl\n\t"
       "pop %0\n\t"
       "mov %0, %1\n\t"

       /* ... Toggle the ID bit in one copy and store */
       /*     to the EFLAGS reg */
       "xor $0x200000, %0\n\t"
       "push %0\n\t"
       "popfl\n\t"

       /* ... Get the (hopefully modified) EFLAGS */
       "pushfl\n\t"
       "pop %0\n\t"
       : "=a" (a), "=c" (c)
       :
       : "cc"
       );

   return a != c;
#endif
}

static void
do_cpuid(unsigned int ax, unsigned int *p)
{
#if 0
    __asm__ volatile(
        "cpuid;"
        : "=a" (p[0]), "=b" (p[1]), "=c" (p[2]), "=d" (p[3])
        :  "0" (ax)
        );
#else
// code from libavcodec:
    __asm__ volatile
        ("mov %%"REG_b", %%"REG_S"\n\t"
         "cpuid\n\t"
         "xchg %%"REG_b", %%"REG_S
         : "=a" (p[0]), "=S" (p[1]),
           "=c" (p[2]), "=d" (p[3])
         : "0" (ax));
#endif
}

void GetCpuCaps( CpuCaps *caps)
{
    unsigned int regs[4];
    unsigned int regs2[4];

    memset(caps, 0, sizeof(*caps));
    caps->isX86=1;
    caps->cl_size=32; /* default */
    if (!has_cpuid()) {
        mp_msg(MSGT_CPUDETECT,MSGL_WARN,"CPUID not supported!??? (maybe an old 486?)\n");
        return;
    }
    do_cpuid(0x00000000, regs); // get _max_ cpuid level and vendor name
    mp_msg(MSGT_CPUDETECT,MSGL_V,"CPU vendor name: %.4s%.4s%.4s  max cpuid level: %d\n",
            (char*) (regs+1),(char*) (regs+3),(char*) (regs+2), regs[0]);
    if (regs[0]>=0x00000001)
    {
        char *tmpstr, *ptmpstr;
        unsigned cl_size;

        do_cpuid(0x00000001, regs2);

        caps->cpuType=(regs2[0] >> 8)&0xf;
        caps->cpuModel=(regs2[0] >> 4)&0xf;

// see AMD64 Architecture Programmer's Manual, Volume 3: General-purpose and
// System Instructions, Table 3-2: Effective family computation, page 120.
        if(caps->cpuType==0xf){
            // use extended family (P4, IA64, K8)
            caps->cpuType=0xf+((regs2[0]>>20)&255);
        }
        if(caps->cpuType==0xf || caps->cpuType==6)
            caps->cpuModel |= ((regs2[0]>>16)&0xf) << 4;

        caps->cpuStepping=regs2[0] & 0xf;

        // general feature flags:
        caps->hasTSC  = (regs2[3] & (1 << 8  )) >>  8; // 0x0000010
        caps->hasMMX  = (regs2[3] & (1 << 23 )) >> 23; // 0x0800000
        caps->hasSSE  = (regs2[3] & (1 << 25 )) >> 25; // 0x2000000
        caps->hasSSE2 = (regs2[3] & (1 << 26 )) >> 26; // 0x4000000
        caps->hasSSE3 = (regs2[2] & 1);        // 0x0000001
        caps->hasSSSE3 = (regs2[2] & (1 << 9 )) >>  9; // 0x0000200
        caps->hasMMX2 = caps->hasSSE; // SSE cpus supports mmxext too
        cl_size = ((regs2[1] >> 8) & 0xFF)*8;
        if(cl_size) caps->cl_size = cl_size;

        ptmpstr=tmpstr=GetCpuFriendlyName(regs, regs2);
        while(*ptmpstr == ' ')    // strip leading spaces
            ptmpstr++;
        mp_msg(MSGT_CPUDETECT,MSGL_V,"CPU: %s ", ptmpstr);
        free(tmpstr);
        mp_msg(MSGT_CPUDETECT,MSGL_V,"(Family: %d, Model: %d, Stepping: %d)\n",
               caps->cpuType, caps->cpuModel, caps->cpuStepping);

    }
    do_cpuid(0x80000000, regs);
    if (regs[0]>=0x80000001) {
        mp_msg(MSGT_CPUDETECT,MSGL_V,"extended cpuid-level: %d\n",regs[0]&0x7FFFFFFF);
        do_cpuid(0x80000001, regs2);
        caps->hasMMX  |= (regs2[3] & (1 << 23 )) >> 23; // 0x0800000
        caps->hasMMX2 |= (regs2[3] & (1 << 22 )) >> 22; // 0x400000
        caps->has3DNow    = (regs2[3] & (1 << 31 )) >> 31; //0x80000000
        caps->has3DNowExt = (regs2[3] & (1 << 30 )) >> 30;
        caps->hasSSE4a = (regs2[2] & (1 << 6 )) >>  6; // 0x0000040
    }
    if(regs[0]>=0x80000006)
    {
        do_cpuid(0x80000006, regs2);
        mp_msg(MSGT_CPUDETECT,MSGL_V,"extended cache-info: %d\n",regs2[2]&0x7FFFFFFF);
        caps->cl_size  = regs2[2] & 0xFF;
    }
    mp_msg(MSGT_CPUDETECT,MSGL_V,"Detected cache-line size is %u bytes\n",caps->cl_size);
#if 0
    mp_msg(MSGT_CPUDETECT,MSGL_INFO,"cpudetect: MMX=%d MMX2=%d SSE=%d SSE2=%d 3DNow=%d 3DNowExt=%d\n",
           gCpuCaps.hasMMX,
           gCpuCaps.hasMMX2,
           gCpuCaps.hasSSE,
           gCpuCaps.hasSSE2,
           gCpuCaps.has3DNow,
           gCpuCaps.has3DNowExt);
#endif

        /* FIXME: Does SSE2 need more OS support, too? */
        if (caps->hasSSE)
            check_os_katmai_support();
        if (!caps->hasSSE)
            caps->hasSSE2 = 0;
//          caps->has3DNow=1;
//          caps->hasMMX2 = 0;
//          caps->hasMMX = 0;

#if !CONFIG_RUNTIME_CPUDETECT
#if !HAVE_MMX
        if(caps->hasMMX) mp_msg(MSGT_CPUDETECT,MSGL_WARN,"MMX supported but disabled\n");
        caps->hasMMX=0;
#endif
#if !HAVE_MMX2
        if(caps->hasMMX2) mp_msg(MSGT_CPUDETECT,MSGL_WARN,"MMX2 supported but disabled\n");
        caps->hasMMX2=0;
#endif
#if !HAVE_SSE
        if(caps->hasSSE) mp_msg(MSGT_CPUDETECT,MSGL_WARN,"SSE supported but disabled\n");
        caps->hasSSE=0;
#endif
#if !HAVE_SSE2
        if(caps->hasSSE2) mp_msg(MSGT_CPUDETECT,MSGL_WARN,"SSE2 supported but disabled\n");
        caps->hasSSE2=0;
#endif
#if !HAVE_AMD3DNOW
        if(caps->has3DNow) mp_msg(MSGT_CPUDETECT,MSGL_WARN,"3DNow supported but disabled\n");
        caps->has3DNow=0;
#endif
#if !HAVE_AMD3DNOWEXT
        if(caps->has3DNowExt) mp_msg(MSGT_CPUDETECT,MSGL_WARN,"3DNowExt supported but disabled\n");
        caps->has3DNowExt=0;
#endif
#endif  // CONFIG_RUNTIME_CPUDETECT
}

char *GetCpuFriendlyName(unsigned int regs[], unsigned int regs2[]){
    char vendor[13];
    char *retname;
    int i;

    if (NULL==(retname=malloc(256))) {
        mp_msg(MSGT_CPUDETECT,MSGL_FATAL,"Error: GetCpuFriendlyName() not enough memory\n");
        exit(1);
    }
    retname[0] = '\0';

    sprintf(vendor,"%.4s%.4s%.4s",(char*)(regs+1),(char*)(regs+3),(char*)(regs+2));

    do_cpuid(0x80000000,regs);
    if (regs[0] >= 0x80000004)
    {
        // CPU has built-in namestring
        for (i = 0x80000002; i <= 0x80000004; i++)
        {
            do_cpuid(i, regs);
            strncat(retname, (char*)regs, 16);
        }
    }
    return retname;
}

#if defined(__linux__) && defined(_POSIX_SOURCE) && !ARCH_X86_64
static void sigill_handler_sse( int signal, struct sigcontext sc )
{
   mp_msg(MSGT_CPUDETECT,MSGL_V, "SIGILL, " );

   /* Both the "xorps %%xmm0,%%xmm0" and "divps %xmm0,%%xmm1"
    * instructions are 3 bytes long.  We must increment the instruction
    * pointer manually to avoid repeated execution of the offending
    * instruction.
    *
    * If the SIGILL is caused by a divide-by-zero when unmasked
    * exceptions aren't supported, the SIMD FPU status and control
    * word will be restored at the end of the test, so we don't need
    * to worry about doing it here.  Besides, we may not be able to...
    */
   sc.eip += 3;

   gCpuCaps.hasSSE=0;
}
#endif /* __linux__ && _POSIX_SOURCE */

#if (defined(__MINGW32__) || defined(__CYGWIN__)) && !ARCH_X86_64
LONG CALLBACK win32_sig_handler_sse(EXCEPTION_POINTERS* ep)
{
    if(ep->ExceptionRecord->ExceptionCode==EXCEPTION_ILLEGAL_INSTRUCTION){
        mp_msg(MSGT_CPUDETECT,MSGL_V, "SIGILL, " );
        ep->ContextRecord->Eip +=3;
        gCpuCaps.hasSSE=0;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif /* defined(__MINGW32__) || defined(__CYGWIN__) */

#ifdef __OS2__
ULONG _System os2_sig_handler_sse(PEXCEPTIONREPORTRECORD       p1,
                                  PEXCEPTIONREGISTRATIONRECORD p2,
                                  PCONTEXTRECORD               p3,
                                  PVOID                        p4)
{
    if(p1->ExceptionNum == XCPT_ILLEGAL_INSTRUCTION){
        mp_msg(MSGT_CPUDETECT, MSGL_V, "SIGILL, ");

        p3->ctx_RegEip += 3;
        gCpuCaps.hasSSE = 0;

        return XCPT_CONTINUE_EXECUTION;
    }
    return XCPT_CONTINUE_SEARCH;
}
#endif

/* If we're running on a processor that can do SSE, let's see if we
 * are allowed to or not.  This will catch 2.4.0 or later kernels that
 * haven't been configured for a Pentium III but are running on one,
 * and RedHat patched 2.2 kernels that have broken exception handling
 * support for user space apps that do SSE.
 */

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__)
#define SSE_SYSCTL_NAME "hw.instruction_sse"
#elif defined(__APPLE__)
#define SSE_SYSCTL_NAME "hw.optional.sse"
#endif

static void check_os_katmai_support( void )
{
#if ARCH_X86_64
    gCpuCaps.hasSSE=1;
    gCpuCaps.hasSSE2=1;
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__) || defined(__APPLE__)
    int has_sse=0, ret;
    size_t len=sizeof(has_sse);

    ret = sysctlbyname(SSE_SYSCTL_NAME, &has_sse, &len, NULL, 0);
    if (ret || !has_sse)
        gCpuCaps.hasSSE=0;

#elif defined(__NetBSD__) || defined (__OpenBSD__)
#if __NetBSD_Version__ >= 105250000 || (defined __OpenBSD__)
    int has_sse, has_sse2, ret, mib[2];
    size_t varlen;

    mib[0] = CTL_MACHDEP;
    mib[1] = CPU_SSE;
    varlen = sizeof(has_sse);

    mp_msg(MSGT_CPUDETECT,MSGL_V, "Testing OS support for SSE... " );
    ret = sysctl(mib, 2, &has_sse, &varlen, NULL, 0);
    gCpuCaps.hasSSE = ret >= 0 && has_sse;
    mp_msg(MSGT_CPUDETECT,MSGL_V, gCpuCaps.hasSSE ? "yes.\n" : "no!\n" );

    mib[1] = CPU_SSE2;
    varlen = sizeof(has_sse2);
    mp_msg(MSGT_CPUDETECT,MSGL_V, "Testing OS support for SSE2... " );
    ret = sysctl(mib, 2, &has_sse2, &varlen, NULL, 0);
    gCpuCaps.hasSSE2 = ret >= 0 && has_sse2;
    mp_msg(MSGT_CPUDETECT,MSGL_V, gCpuCaps.hasSSE2 ? "yes.\n" : "no!\n" );
#else
    gCpuCaps.hasSSE = 0;
    mp_msg(MSGT_CPUDETECT,MSGL_WARN, "No OS support for SSE, disabling to be safe.\n" );
#endif
#elif defined(__MINGW32__) || defined(__CYGWIN__)
    LPTOP_LEVEL_EXCEPTION_FILTER exc_fil;
    if ( gCpuCaps.hasSSE ) {
        mp_msg(MSGT_CPUDETECT,MSGL_V, "Testing OS support for SSE... " );
        exc_fil = SetUnhandledExceptionFilter(win32_sig_handler_sse);
        __asm__ volatile ("xorps %xmm0, %xmm0");
        SetUnhandledExceptionFilter(exc_fil);
        mp_msg(MSGT_CPUDETECT,MSGL_V, gCpuCaps.hasSSE ? "yes.\n" : "no!\n" );
    }
#elif defined(__OS2__)
    EXCEPTIONREGISTRATIONRECORD RegRec = { 0, &os2_sig_handler_sse };
    if ( gCpuCaps.hasSSE ) {
        mp_msg(MSGT_CPUDETECT,MSGL_V, "Testing OS support for SSE... " );
        DosSetExceptionHandler( &RegRec );
        __asm__ volatile ("xorps %xmm0, %xmm0");
        DosUnsetExceptionHandler( &RegRec );
        mp_msg(MSGT_CPUDETECT,MSGL_V, gCpuCaps.hasSSE ? "yes.\n" : "no!\n" );
    }
#elif defined(__linux__)
#if defined(_POSIX_SOURCE)
    struct sigaction saved_sigill;

    /* Save the original signal handlers.
     */
    sigaction( SIGILL, NULL, &saved_sigill );

    signal( SIGILL, (void (*)(int))sigill_handler_sse );

    /* Emulate test for OSFXSR in CR4.  The OS will set this bit if it
     * supports the extended FPU save and restore required for SSE.  If
     * we execute an SSE instruction on a PIII and get a SIGILL, the OS
     * doesn't support Streaming SIMD Exceptions, even if the processor
     * does.
     */
    if ( gCpuCaps.hasSSE ) {
        mp_msg(MSGT_CPUDETECT,MSGL_V, "Testing OS support for SSE... " );

//      __asm__ volatile ("xorps %%xmm0, %%xmm0");
        __asm__ volatile ("xorps %xmm0, %xmm0");

        mp_msg(MSGT_CPUDETECT,MSGL_V, gCpuCaps.hasSSE ? "yes.\n" : "no!\n" );
    }

    /* Restore the original signal handlers.
     */
    sigaction( SIGILL, &saved_sigill, NULL );

    /* If we've gotten to here and the XMM CPUID bit is still set, we're
     * safe to go ahead and hook out the SSE code throughout Mesa.
     */
    mp_msg(MSGT_CPUDETECT,MSGL_V, "Tests of OS support for SSE %s\n", gCpuCaps.hasSSE ? "passed." : "failed!" );
#else
    /* We can't use POSIX signal handling to test the availability of
     * SSE, so we disable it by default.
     */
    mp_msg(MSGT_CPUDETECT,MSGL_WARN, "Cannot test OS support for SSE, disabling to be safe.\n" );
    gCpuCaps.hasSSE=0;
#endif /* _POSIX_SOURCE */
#else
    /* Do nothing on other platforms for now.
     */
    mp_msg(MSGT_CPUDETECT,MSGL_WARN, "Cannot test OS support for SSE, leaving disabled.\n" );
    gCpuCaps.hasSSE=0;
#endif /* __linux__ */
}
#else /* ARCH_X86 */

#ifdef __APPLE__
#include <sys/sysctl.h>
#elif defined(__AMIGAOS4__)
/* nothing */
#else
#include <signal.h>
#include <setjmp.h>

static sigjmp_buf jmpbuf;
static volatile sig_atomic_t canjump = 0;

static void sigill_handler (int sig)
{
    if (!canjump) {
        signal (sig, SIG_DFL);
        raise (sig);
    }

    canjump = 0;
    siglongjmp (jmpbuf, 1);
}
#endif /* __APPLE__ */

void GetCpuCaps( CpuCaps *caps)
{
    caps->cpuType=0;
    caps->cpuModel=0;
    caps->cpuStepping=0;
    caps->hasMMX=0;
    caps->hasMMX2=0;
    caps->has3DNow=0;
    caps->has3DNowExt=0;
    caps->hasSSE=0;
    caps->hasSSE2=0;
    caps->hasSSE3=0;
    caps->hasSSSE3=0;
    caps->hasSSE4a=0;
    caps->isX86=0;
    caps->hasAltiVec = 0;
#if HAVE_ALTIVEC
#ifdef __APPLE__
/*
  rip-off from ffmpeg altivec detection code.
  this code also appears on Apple's AltiVec pages.
 */
    {
        int sels[2] = {CTL_HW, HW_VECTORUNIT};
        int has_vu = 0;
        size_t len = sizeof(has_vu);
        int err;

        err = sysctl(sels, 2, &has_vu, &len, NULL, 0);

        if (err == 0)
            if (has_vu != 0)
                caps->hasAltiVec = 1;
    }
#elif defined(__AMIGAOS4__)
    ULONG result = 0;

    GetCPUInfoTags(GCIT_VectorUnit, &result, TAG_DONE);
    if (result == VECTORTYPE_ALTIVEC)
        caps->hasAltiVec = 1;
#else
/* no Darwin, do it the brute-force way */
/* this is borrowed from the libmpeg2 library */
    {
        signal (SIGILL, sigill_handler);
        if (sigsetjmp (jmpbuf, 1)) {
            signal (SIGILL, SIG_DFL);
        } else {
            canjump = 1;

        __asm__ volatile ("mtspr 256, %0\n\t"
                          "vand %%v0, %%v0, %%v0"
                          :
                          : "r" (-1));

        signal (SIGILL, SIG_DFL);
        caps->hasAltiVec = 1;
        }
    }
#endif /* __APPLE__ */
    mp_msg(MSGT_CPUDETECT,MSGL_V,"AltiVec %sfound\n", (caps->hasAltiVec ? "" : "not "));
#endif /* HAVE_ALTIVEC */

    if (ARCH_IA64)
        mp_msg(MSGT_CPUDETECT,MSGL_V,"CPU: Intel Itanium\n");

    if (ARCH_SPARC)
        mp_msg(MSGT_CPUDETECT,MSGL_V,"CPU: Sun Sparc\n");

    if (ARCH_ARM)
        mp_msg(MSGT_CPUDETECT,MSGL_V,"CPU: ARM\n");

    if (ARCH_PPC)
        mp_msg(MSGT_CPUDETECT,MSGL_V,"CPU: PowerPC\n");

    if (ARCH_ALPHA)
        mp_msg(MSGT_CPUDETECT,MSGL_V,"CPU: Digital Alpha\n");

    if (ARCH_MIPS)
        mp_msg(MSGT_CPUDETECT,MSGL_V,"CPU: MIPS\n");

    if (ARCH_PA_RISC)
        mp_msg(MSGT_CPUDETECT,MSGL_V,"CPU: Hewlett-Packard PA-RISC\n");

    if (ARCH_S390)
        mp_msg(MSGT_CPUDETECT,MSGL_V,"CPU: IBM S/390\n");

    if (ARCH_S390X)
        mp_msg(MSGT_CPUDETECT,MSGL_V,"CPU: IBM S/390X\n");

    if (ARCH_VAX)
        mp_msg(MSGT_CPUDETECT,MSGL_V, "CPU: Digital VAX\n" );

    if (ARCH_XTENSA)
        mp_msg(MSGT_CPUDETECT,MSGL_V, "CPU: Tensilica Xtensa\n" );
}
#endif /* !ARCH_X86 */
