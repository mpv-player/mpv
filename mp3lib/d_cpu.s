
/ ---------------------------------------------------------------------------
/  Cpu function detect by Pontscho/fresh!mindworkz (c) 2000 - 2000
/  3dnow-dsp detection by Nick Kurshev (C) 2001
/ ---------------------------------------------------------------------------

.text

.globl CpuDetect
.globl ipentium
.globl a3dnow
.globl isse

/ ---------------------------------------------------------------------------
/  in C: unsigned long CpuDetect( void );
/   return: cpu ident number.
/ ---------------------------------------------------------------------------
CpuDetect:
        pushl %ebx
        pushl %ecx
        pushl %edx

        pushfl
        popl   %eax
        movl   %eax,%ebx
        xorl   $0x00200000,%eax
        pushl  %eax
        popfl
        pushfl
        popl   %eax
        cmpl   %eax,%ebx
        jz     no_cpuid_cpudetect
	
        movl   $1,%eax
        cpuid
	
        jmp    exit_cpudetect
no_cpuid_cpudetect:
        xorl   %eax,%eax
exit_cpudetect:

        popl  %edx
        popl  %ecx
        popl  %ebx
        ret

/ ---------------------------------------------------------------------------
/  in C: unsigled long ipentium( void );
/  return: 0 if this processor i386 or i486
/          1 otherwise
/          2 if this cpu supports mmx
/ ---------------------------------------------------------------------------
ipentium:
        pushl  %ebx
        pushl  %ecx
        pushl  %edx
        pushfl
        popl   %eax
        movl   %eax,%ebx
        xorl   $0x00200000,%eax
        pushl  %eax
        popfl
        pushfl
        popl   %eax
        cmpl   %eax,%ebx
        jz     no_cpuid
        movl   $1,%eax
        cpuid
	movl   %eax, %ecx
	xorl   %eax, %eax
        shrl   $8,%ecx
        cmpl   $5,%ecx
        jb     exit
        incl   %eax
	test   $0x00800000, %edx
	jz     exit
	incl   %eax
        jmp    exit
no_cpuid:
        xorl   %eax,%eax
exit:
        popl   %edx
        popl   %ecx
        popl   %ebx
        ret

/ ---------------------------------------------------------------------------
/  in C: unsigned long a3dnow( void );
/  return: 0 if this processor does not support 3dnow!
/          1 otherwise
/          2 if this cpu supports 3dnow-dsp extension
/ ---------------------------------------------------------------------------
a3dnow:
        pushl  %ebx
        pushl  %edx
        pushl  %ecx


        call   ipentium
        testl  %eax,%eax
        jz     exit2

        movl   $0x80000000,%eax
        cpuid
        cmpl   $0x80000000,%eax
        jbe    exit2
        movl   $0x80000001,%eax
        cpuid
        xorl   %eax,%eax
        testl  $0x80000000,%edx
        jz     exit2
/// eax=1 - K6 3DNow!
        inc    %eax
        testl  $0x40000000,%edx
        jz     exit2
/// eax=2 - K7 3DNowEx!	
        inc    %eax
exit2:

        popl   %ecx
        popl   %edx
        popl   %ebx
        ret

/ ---------------------------------------------------------------------------
/  in C: unsigned long isse( void );
/  return: 0 if this processor does not support sse
/          1 otherwise
/          2 if this cpu supports sse2 extension
/ ---------------------------------------------------------------------------
isse:
        pushl  %ebx
        pushl  %edx
        pushl  %ecx

        call   ipentium
        testl  %eax,%eax
        jz     exit3

        movl   $1,%eax
        cpuid
	xorl   %eax, %eax
        testl  $0x02000000,%edx
        jz     exit3
	incl   %eax
        testl  $0x04000000,%edx
        jz     exit3
        incl   %eax
exit3:
        popl   %ecx
        popl   %edx
        popl   %ebx
        ret
