
/ ---------------------------------------------------------------------------
/  Cpu function detect by Pontscho/fresh!mindworkz
/   (c) 2000 - 2000
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

        movl  $1,%eax
        cpuid

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
/   return: 0 if this processor not requiment 3dnow! else above 1.
/ ---------------------------------------------------------------------------
a3dnow:
        pushl  %ebx
        pushl  %edx
        pushl  %ecx


        call   ipentium
        shrl   $1,%eax
        jnc    no_3dnow

        movl   $0x80000000,%eax
        cpuid
        cmpl   $0x80000000,%eax
        jbe    no_3dnow
        movl   $0x80000001,%eax
        cpuid
        testl  $0x80000000,%edx
        jz     no_3dnow
        movl   $1,%eax
        jmp    exit2
no_3dnow:
        xorl   %eax,%eax
exit2:

        popl   %ecx
        popl   %edx
        popl   %ebx
        ret
