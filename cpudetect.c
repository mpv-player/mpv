#include "config.h"
#include "cpudetect.h"

#ifdef ARCH_X86

#include <stdio.h>

#ifdef __FreeBSD__
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#ifdef __linux__
#include <signal.h>
#endif

//#define X86_FXSR_MAGIC
/* Thanks to the FreeBSD project for some of this cpuid code, and 
 * help understanding how to use it.  Thanks to the Mesa 
 * team for SSE support detection and more cpu detect code.
 */

/* I believe this code works.  However, it has only been used on a PII and PIII */

CpuCaps gCpuCaps;
static void check_os_katmai_support( void );

#if 1
// return TRUE if cpuid supported
static int has_cpuid()
{
	int a, c;

// code from libavcodec:
    __asm__ __volatile__ (
                          /* See if CPUID instruction is supported ... */
                          /* ... Get copies of EFLAGS into eax and ecx */
                          "pushf\n\t"
                          "popl %0\n\t"
                          "movl %0, %1\n\t"
                          
                          /* ... Toggle the ID bit in one copy and store */
                          /*     to the EFLAGS reg */
                          "xorl $0x200000, %0\n\t"
                          "push %0\n\t"
                          "popf\n\t"
                          
                          /* ... Get the (hopefully modified) EFLAGS */
                          "pushf\n\t"
                          "popl %0\n\t"
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
	("movl %%ebx, %%esi\n\t"
         "cpuid\n\t"
         "xchgl %%ebx, %%esi"
         : "=a" (p[0]), "=S" (p[1]),
           "=c" (p[2]), "=d" (p[3])
         : "0" (ax));
#endif

}


void GetCpuCaps( CpuCaps *caps)
{
	unsigned int regs[4];
	unsigned int regs2[4];
	
	bzero(caps, sizeof(*caps));
	if (!has_cpuid()) {
	    printf("CPUID not supported!???\n");
	    return;
	}
	do_cpuid(0x00000000, regs); // get _max_ cpuid level and vendor name
	printf("CPU vendor name: %.4s%.4s%.4s  max cpuid level: %d\n",&regs[1],&regs[3],&regs[2],regs[0]);
	if (regs[0]>=0x00000001)
	{
		do_cpuid(0x00000001, regs2);
		caps->cpuType=(regs2[0] >> 8)&0xf;
		if(caps->cpuType==0xf){
		    // use extended family (P4, IA64)
		    caps->cpuType=8+((regs2[0]>>20)&255);
		}

		// general feature flags:
		caps->hasMMX  = (regs2[3] & (1 << 23 )) >> 23; // 0x0800000
		caps->hasSSE  = (regs2[3] & (1 << 25 )) >> 25; // 0x2000000
		caps->hasSSE2 = (regs2[3] & (1 << 26 )) >> 26; // 0x4000000
		caps->hasMMX2 = caps->hasSSE; // SSE cpus supports mmxext too
	}
	do_cpuid(0x80000000, regs);
	if (regs[0]>=0x80000001) {
		printf("extended cpuid-level: %d\n",regs[0]&0x7FFFFFFF);
		do_cpuid(0x80000001, regs2);
		caps->hasMMX  = (regs2[3] & (1 << 23 )) >> 23; // 0x0800000
		caps->hasMMX2 = (regs2[3] & (1 << 22 )) >> 22; // 0x400000
		caps->has3DNow    = (regs2[3] & (1 << 31 )) >> 31; //0x80000000
		caps->has3DNowExt = (regs2[3] & (1 << 30 )) >> 30;
	}
#if 0
	printf("cpudetect: MMX=%d MMX2=%d SSE=%d SSE2=%d 3DNow=%d 3DNowExt=%d\n",
		gCpuCaps.hasMMX,
		gCpuCaps.hasMMX2,
		gCpuCaps.hasSSE,
		gCpuCaps.hasSSE2,
		gCpuCaps.has3DNow,
		gCpuCaps.has3DNowExt );
#endif

		/* FIXME: Does SSE2 need more OS support, too? */
#if defined(__linux__) || defined(__FreeBSD__)
		if (caps->hasSSE)
			check_os_katmai_support();
		if (!caps->hasSSE)
			caps->hasSSE2 = 0;
#else
		caps->hasSSE=0;
		caps->hasSSE2 = 0;
#endif


}

#if defined(__linux__) && defined(_POSIX_SOURCE) && defined(X86_FXSR_MAGIC)
static void sigill_handler_sse( int signal, struct sigcontext sc )
{
   printf( "SIGILL, " );

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
   printf( "SIGFPE, " );

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
      printf( "\n\n" );
      printf( "SSE enabling test failed badly!" );
   }
}
#endif /* __linux__ && _POSIX_SOURCE && X86_FXSR_MAGIC */

/* If we're running on a processor that can do SSE, let's see if we
 * are allowed to or not.  This will catch 2.4.0 or later kernels that
 * haven't been configured for a Pentium III but are running on one,
 * and RedHat patched 2.2 kernels that have broken exception handling
 * support for user space apps that do SSE.
 */
static void check_os_katmai_support( void )
{
#if defined(__FreeBSD__)
   int has_sse=0, ret;
   size_t len=sizeof(has_sse);

   ret = sysctlbyname("hw.instruction_sse", &has_sse, &len, NULL, 0);
   if (ret || !has_sse)
      gCpuCaps.hasSSE=0;

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
      printf( "Testing OS support for SSE... " );

//      __asm __volatile ("xorps %%xmm0, %%xmm0");
      __asm __volatile ("xorps %xmm0, %xmm0");

      if ( gCpuCaps.hasSSE ) {
	 printf( "yes.\n" );
      } else {
	 printf( "no!\n" );
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
      printf( "Testing OS support for SSE unmasked exceptions... " );

//      test_os_katmai_exception_support();

      if ( gCpuCaps.hasSSE ) {
	 printf( "yes.\n" );
      } else {
	 printf( "no!\n" );
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
      printf( "Tests of OS support for SSE passed.\n" );
   } else {
      printf( "Tests of OS support for SSE failed!\n" );
   }
#else
   /* We can't use POSIX signal handling to test the availability of
    * SSE, so we disable it by default.
    */
   printf( "Cannot test OS support for SSE, disabling to be safe.\n" );
   gCpuCaps.hasSSE=0;
#endif /* _POSIX_SOURCE && X86_FXSR_MAGIC */
#else
   /* Do nothing on other platforms for now.
    */
   message( "Not testing OS support for SSE, leaving disabled.\n" );
   gCpuCaps.hasSSE=0;
#endif /* __linux__ */
}
#endif /* ARCH_X86 */
