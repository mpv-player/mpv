#include "config.h"
#include "cpudetect.h"
#include "mp_msg.h"

CpuCaps gCpuCaps;

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <stdlib.h>

#ifdef ARCH_X86

#include <stdio.h>
#include <string.h>

#if defined (__NetBSD__) || defined(__OpenBSD__)
#include <sys/param.h>
#include <sys/sysctl.h>
#include <machine/cpu.h>
#endif

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#ifdef __linux__
#include <signal.h>
#endif

#ifdef WIN32
#include <windows.h>
#endif

#ifdef __AMIGAOS4__
#include <proto/exec.h>
#endif

//#define X86_FXSR_MAGIC
/* Thanks to the FreeBSD project for some of this cpuid code, and 
 * help understanding how to use it.  Thanks to the Mesa 
 * team for SSE support detection and more cpu detect code.
 */

/* I believe this code works.  However, it has only been used on a PII and PIII */

static void check_os_katmai_support( void );

#if 1
// return TRUE if cpuid supported
static int has_cpuid(void)
{
	long a, c;

// code from libavcodec:
    __asm__ __volatile__ (
                          /* See if CPUID instruction is supported ... */
                          /* ... Get copies of EFLAGS into eax and ecx */
                          "pushf\n\t"
                          "pop %0\n\t"
                          "mov %0, %1\n\t"
                          
                          /* ... Toggle the ID bit in one copy and store */
                          /*     to the EFLAGS reg */
                          "xor $0x200000, %0\n\t"
                          "push %0\n\t"
                          "popf\n\t"
                          
                          /* ... Get the (hopefully modified) EFLAGS */
                          "pushf\n\t"
                          "pop %0\n\t"
                          : "=a" (a), "=c" (c)
                          :
                          : "cc" 
                          );

	return (a!=c);
}
#endif

static void
do_cpuid(unsigned int ax, unsigned int *p)
{
#if 0
	__asm __volatile(
	"cpuid;"
	: "=a" (p[0]), "=b" (p[1]), "=c" (p[2]), "=d" (p[3])
	:  "0" (ax)
	);
#else
// code from libavcodec:
    __asm __volatile
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
		caps->hasMMX2 = caps->hasSSE; // SSE cpus supports mmxext too
		cl_size = ((regs2[1] >> 8) & 0xFF)*8;
		if(cl_size) caps->cl_size = cl_size;

		ptmpstr=tmpstr=GetCpuFriendlyName(regs, regs2);
		while(*ptmpstr == ' ')        // strip leading spaces
		    ptmpstr++;
		mp_msg(MSGT_CPUDETECT,MSGL_INFO,"CPU: %s ", ptmpstr);
		free(tmpstr);
		mp_msg(MSGT_CPUDETECT,MSGL_INFO,"(Family: %d, Model: %d, Stepping: %d)\n",
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
		gCpuCaps.has3DNowExt );
#endif

		/* FIXME: Does SSE2 need more OS support, too? */
#if defined(__linux__) || defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__NetBSD__) || defined(__CYGWIN__) || defined(__OpenBSD__) || defined(__DragonFly__) || defined(__APPLE__)
		if (caps->hasSSE)
			check_os_katmai_support();
		if (!caps->hasSSE)
			caps->hasSSE2 = 0;
#else
		caps->hasSSE=0;
		caps->hasSSE2 = 0;
#endif
//		caps->has3DNow=1;
//		caps->hasMMX2 = 0;
//		caps->hasMMX = 0;

#ifndef HAVE_MMX
	if(caps->hasMMX) mp_msg(MSGT_CPUDETECT,MSGL_WARN,"MMX supported but disabled\n");
	caps->hasMMX=0;
#endif
#ifndef HAVE_MMX2
	if(caps->hasMMX2) mp_msg(MSGT_CPUDETECT,MSGL_WARN,"MMX2 supported but disabled\n");
	caps->hasMMX2=0;
#endif
#ifndef HAVE_SSE
	if(caps->hasSSE) mp_msg(MSGT_CPUDETECT,MSGL_WARN,"SSE supported but disabled\n");
	caps->hasSSE=0;
#endif
#ifndef HAVE_SSE2
	if(caps->hasSSE2) mp_msg(MSGT_CPUDETECT,MSGL_WARN,"SSE2 supported but disabled\n");
	caps->hasSSE2=0;
#endif
#ifndef HAVE_3DNOW
	if(caps->has3DNow) mp_msg(MSGT_CPUDETECT,MSGL_WARN,"3DNow supported but disabled\n");
	caps->has3DNow=0;
#endif
#ifndef HAVE_3DNOWEX
	if(caps->has3DNowExt) mp_msg(MSGT_CPUDETECT,MSGL_WARN,"3DNowExt supported but disabled\n");
	caps->has3DNowExt=0;
#endif
}


#define CPUID_EXTFAMILY	((regs2[0] >> 20)&0xFF) /* 27..20 */
#define CPUID_EXTMODEL	((regs2[0] >> 16)&0x0F) /* 19..16 */
#define CPUID_TYPE		((regs2[0] >> 12)&0x04) /* 13..12 */
#define CPUID_FAMILY	((regs2[0] >>  8)&0x0F) /* 11..08 */
#define CPUID_MODEL		((regs2[0] >>  4)&0x0F) /* 07..04 */
#define CPUID_STEPPING	((regs2[0] >>  0)&0x0F) /* 03..00 */

char *GetCpuFriendlyName(unsigned int regs[], unsigned int regs2[]){
#include "cputable.h" /* get cpuname and cpuvendors */
	char vendor[13];
	char *retname;
	int i;

	if (NULL==(retname=malloc(256))) {
		mp_msg(MSGT_CPUDETECT,MSGL_FATAL,"Error: GetCpuFriendlyName() not enough memory\n");
		exit(1);
	}

	sprintf(vendor,"%.4s%.4s%.4s",(char*)(regs+1),(char*)(regs+3),(char*)(regs+2));

	do_cpuid(0x80000000,regs);
	if (regs[0] >= 0x80000004)
	{
		// CPU has built-in namestring
		retname[0] = '\0';
		for (i = 0x80000002; i <= 0x80000004; i++)
		{
			do_cpuid(i, regs);
			strncat(retname, (char*)regs, 16);
		}
		return retname;
	}

	for(i=0; i<MAX_VENDORS; i++){
		if(!strcmp(cpuvendors[i].string,vendor)){
			if(cpuname[i][CPUID_FAMILY][CPUID_MODEL]){
				snprintf(retname,255,"%s %s",cpuvendors[i].name,cpuname[i][CPUID_FAMILY][CPUID_MODEL]);
			} else {
				snprintf(retname,255,"unknown %s %d. Generation CPU",cpuvendors[i].name,CPUID_FAMILY); 
				mp_msg(MSGT_CPUDETECT,MSGL_WARN,"unknown %s CPU:\n",cpuvendors[i].name);
				mp_msg(MSGT_CPUDETECT,MSGL_WARN,"Vendor:   %s\n",cpuvendors[i].string);
				mp_msg(MSGT_CPUDETECT,MSGL_WARN,"Type:     %d\n",CPUID_TYPE);
				mp_msg(MSGT_CPUDETECT,MSGL_WARN,"Family:   %d (ext: %d)\n",CPUID_FAMILY,CPUID_EXTFAMILY);
				mp_msg(MSGT_CPUDETECT,MSGL_WARN,"Model:    %d (ext: %d)\n",CPUID_MODEL,CPUID_EXTMODEL);
				mp_msg(MSGT_CPUDETECT,MSGL_WARN,"Stepping: %d\n",CPUID_STEPPING);
				mp_msg(MSGT_CPUDETECT,MSGL_WARN,"Please send the above info along with the exact CPU name"
				       "to the MPlayer-Developers, so we can add it to the list!\n");
			}
		}
	}
	retname[255] = 0;

	//printf("Detected CPU: %s\n", retname);
	return retname;
}

#undef CPUID_EXTFAMILY
#undef CPUID_EXTMODEL
#undef CPUID_TYPE
#undef CPUID_FAMILY
#undef CPUID_MODEL
#undef CPUID_STEPPING


#if defined(__linux__) && defined(_POSIX_SOURCE) && defined(X86_FXSR_MAGIC)
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

static void sigfpe_handler_sse( int signal, struct sigcontext sc )
{
   mp_msg(MSGT_CPUDETECT,MSGL_V, "SIGFPE, " );

   if ( sc.fpstate->magic != 0xffff ) {
      /* Our signal context has the extended FPU state, so reset the
       * divide-by-zero exception mask and clear the divide-by-zero
       * exception bit.
       */
      sc.fpstate->mxcsr |= 0x00000200;
      sc.fpstate->mxcsr &= 0xfffffffb;
   } else {
      /* If we ever get here, we're completely hosed.
       */
      mp_msg(MSGT_CPUDETECT,MSGL_V, "\n\n" );
      mp_msg(MSGT_CPUDETECT,MSGL_V, "SSE enabling test failed badly!" );
   }
}
#endif /* __linux__ && _POSIX_SOURCE && X86_FXSR_MAGIC */

#ifdef WIN32
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
#endif /* WIN32 */

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
#ifdef ARCH_X86_64
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
   if (ret < 0 || !has_sse) {
      gCpuCaps.hasSSE=0;
      mp_msg(MSGT_CPUDETECT,MSGL_V, "no!\n" );
   } else {
      gCpuCaps.hasSSE=1;
      mp_msg(MSGT_CPUDETECT,MSGL_V, "yes!\n" );
   }

   mib[1] = CPU_SSE2;
   varlen = sizeof(has_sse2);
   mp_msg(MSGT_CPUDETECT,MSGL_V, "Testing OS support for SSE2... " );
   ret = sysctl(mib, 2, &has_sse2, &varlen, NULL, 0);
   if (ret < 0 || !has_sse2) {
      gCpuCaps.hasSSE2=0;
      mp_msg(MSGT_CPUDETECT,MSGL_V, "no!\n" );
   } else {
      gCpuCaps.hasSSE2=1;
      mp_msg(MSGT_CPUDETECT,MSGL_V, "yes!\n" );
   }
#else
   gCpuCaps.hasSSE = 0;
   mp_msg(MSGT_CPUDETECT,MSGL_WARN, "No OS support for SSE, disabling to be safe.\n" );
#endif
#elif defined(WIN32)
   LPTOP_LEVEL_EXCEPTION_FILTER exc_fil;
   if ( gCpuCaps.hasSSE ) {
      mp_msg(MSGT_CPUDETECT,MSGL_V, "Testing OS support for SSE... " );
      exc_fil = SetUnhandledExceptionFilter(win32_sig_handler_sse);
      __asm __volatile ("xorps %xmm0, %xmm0");
      SetUnhandledExceptionFilter(exc_fil);
      if ( gCpuCaps.hasSSE ) mp_msg(MSGT_CPUDETECT,MSGL_V, "yes.\n" );
      else mp_msg(MSGT_CPUDETECT,MSGL_V, "no!\n" );
   }
#elif defined(__linux__)
#if defined(_POSIX_SOURCE) && defined(X86_FXSR_MAGIC)
   struct sigaction saved_sigill;
   struct sigaction saved_sigfpe;

   /* Save the original signal handlers.
    */
   sigaction( SIGILL, NULL, &saved_sigill );
   sigaction( SIGFPE, NULL, &saved_sigfpe );

   signal( SIGILL, (void (*)(int))sigill_handler_sse );
   signal( SIGFPE, (void (*)(int))sigfpe_handler_sse );

   /* Emulate test for OSFXSR in CR4.  The OS will set this bit if it
    * supports the extended FPU save and restore required for SSE.  If
    * we execute an SSE instruction on a PIII and get a SIGILL, the OS
    * doesn't support Streaming SIMD Exceptions, even if the processor
    * does.
    */
   if ( gCpuCaps.hasSSE ) {
      mp_msg(MSGT_CPUDETECT,MSGL_V, "Testing OS support for SSE... " );

//      __asm __volatile ("xorps %%xmm0, %%xmm0");
      __asm __volatile ("xorps %xmm0, %xmm0");

      if ( gCpuCaps.hasSSE ) {
	 mp_msg(MSGT_CPUDETECT,MSGL_V, "yes.\n" );
      } else {
	 mp_msg(MSGT_CPUDETECT,MSGL_V, "no!\n" );
      }
   }

   /* Emulate test for OSXMMEXCPT in CR4.  The OS will set this bit if
    * it supports unmasked SIMD FPU exceptions.  If we unmask the
    * exceptions, do a SIMD divide-by-zero and get a SIGILL, the OS
    * doesn't support unmasked SIMD FPU exceptions.  If we get a SIGFPE
    * as expected, we're okay but we need to clean up after it.
    *
    * Are we being too stringent in our requirement that the OS support
    * unmasked exceptions?  Certain RedHat 2.2 kernels enable SSE by
    * setting CR4.OSFXSR but don't support unmasked exceptions.  Win98
    * doesn't even support them.  We at least know the user-space SSE
    * support is good in kernels that do support unmasked exceptions,
    * and therefore to be safe I'm going to leave this test in here.
    */
   if ( gCpuCaps.hasSSE ) {
      mp_msg(MSGT_CPUDETECT,MSGL_V, "Testing OS support for SSE unmasked exceptions... " );

//      test_os_katmai_exception_support();

      if ( gCpuCaps.hasSSE ) {
	 mp_msg(MSGT_CPUDETECT,MSGL_V, "yes.\n" );
      } else {
	 mp_msg(MSGT_CPUDETECT,MSGL_V, "no!\n" );
      }
   }

   /* Restore the original signal handlers.
    */
   sigaction( SIGILL, &saved_sigill, NULL );
   sigaction( SIGFPE, &saved_sigfpe, NULL );

   /* If we've gotten to here and the XMM CPUID bit is still set, we're
    * safe to go ahead and hook out the SSE code throughout Mesa.
    */
   if ( gCpuCaps.hasSSE ) {
      mp_msg(MSGT_CPUDETECT,MSGL_V, "Tests of OS support for SSE passed.\n" );
   } else {
      mp_msg(MSGT_CPUDETECT,MSGL_V, "Tests of OS support for SSE failed!\n" );
   }
#else
   /* We can't use POSIX signal handling to test the availability of
    * SSE, so we disable it by default.
    */
   mp_msg(MSGT_CPUDETECT,MSGL_WARN, "Cannot test OS support for SSE, disabling to be safe.\n" );
   gCpuCaps.hasSSE=0;
#endif /* _POSIX_SOURCE && X86_FXSR_MAGIC */
#else
   /* Do nothing on other platforms for now.
    */
   mp_msg(MSGT_CPUDETECT,MSGL_WARN, "Cannot test OS support for SSE, leaving disabled.\n" );
   gCpuCaps.hasSSE=0;
#endif /* __linux__ */
}
#else /* ARCH_X86 */

#ifdef SYS_DARWIN
#include <sys/sysctl.h>
#else
#ifndef __AMIGAOS4__
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
#endif //__AMIGAOS4__
#endif

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
	caps->isX86=0;
	caps->hasAltiVec = 0;
#ifdef HAVE_ALTIVEC   
#ifdef SYS_DARWIN   
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
#else /* SYS_DARWIN */
#ifdef __AMIGAOS4__
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
            
            asm volatile ("mtspr 256, %0\n\t"
                          "vand %%v0, %%v0, %%v0"
                          :
                          : "r" (-1));
            
            signal (SIGILL, SIG_DFL);
            caps->hasAltiVec = 1;
          }
        }
#endif //__AMIGAOS4__
#endif /* SYS_DARWIN */
        mp_msg(MSGT_CPUDETECT,MSGL_INFO,"AltiVec %sfound\n", (caps->hasAltiVec ? "" : "not "));
#endif /* HAVE_ALTIVEC */

#ifdef ARCH_IA64
	mp_msg(MSGT_CPUDETECT,MSGL_INFO,"CPU: Intel Itanium\n");
#endif

#ifdef ARCH_SPARC
	mp_msg(MSGT_CPUDETECT,MSGL_INFO,"CPU: Sun Sparc\n");
#endif

#ifdef ARCH_ARMV4L
	mp_msg(MSGT_CPUDETECT,MSGL_INFO,"CPU: ARM\n");
#endif

#ifdef ARCH_POWERPC
	mp_msg(MSGT_CPUDETECT,MSGL_INFO,"CPU: PowerPC\n");
#endif

#ifdef ARCH_ALPHA
	mp_msg(MSGT_CPUDETECT,MSGL_INFO,"CPU: Digital Alpha\n");
#endif

#ifdef ARCH_SGI_MIPS
	mp_msg(MSGT_CPUDETECT,MSGL_INFO,"CPU: SGI MIPS\n");
#endif

#ifdef ARCH_PA_RISC
	mp_msg(MSGT_CPUDETECT,MSGL_INFO,"CPU: Hewlett-Packard PA-RISC\n");
#endif

#ifdef ARCH_S390
	mp_msg(MSGT_CPUDETECT,MSGL_INFO,"CPU: IBM S/390\n");
#endif

#ifdef ARCH_S390X
	mp_msg(MSGT_CPUDETECT,MSGL_INFO,"CPU: IBM S/390X\n");
#endif

#ifdef ARCH_VAX
	mp_msg(MSGT_CPUDETECT,MSGL_INFO, "CPU: Digital VAX\n" );
#endif
}
#endif /* !ARCH_X86 */
