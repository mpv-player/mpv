
/ ---------------------------------------------------------------------------
/  Cpu function detect by Pontscho/fresh!mindworkz (c) 2000 - 2000
/  3dnow-dsp detection by Nick Kurshev (C) 2001
/ ---------------------------------------------------------------------------

.text

.globl CpuDetect
.globl ipentium
.globl a3dnow

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
/   return: 0 if the processor is not P5 or above else above 1.
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
        shrl   $8,%eax
        cmpl   $5,%eax
        jb     no_cpuid
        movl   $1,%eax
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
