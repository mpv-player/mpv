///
/// Replacement of synth_1to1() with AMD's 3DNowEx(DSP)! SIMD operations support
///
/// This code based 'decode_3dnow.s' by Syuuhei Kashiyama
/// <squash@mb.kcom.ne.jp>,only some types of changes have been made:
///
///  - Added new opcode PFNACC
///  - decreased number of opcodes (as it was suggested by k7 manual)
///    (using memory reference as operand of instructions)
///  - added PREFETCHW opcode. It has different semantic on k7 than on k6-2
///    and saves 15-25 cpu clocks for athlon.
///  - partial unrolling loops for removing slower MOVW insns.
///    (Note: probably same operation should be done for decode_3dnow.s)
///  - change function name for support 3DNowEx! automatic detect
///  - added loops alignment
///
/// note: because K7 processors are an aggresive out-of-order three-way
///       superscalar ones instruction order is not significand for them.
///
/// Benchmark: measured by mplayer on Duron-700:
///      3dNow! optimized code                              - 1.4% of cpu usage
///      k7 optimized code (without partial loop unrolling) - 1.3% of cpu usage
///      k7 optimized code                                  - 1.1% of cpu usage
/// Note: K6-2 users have an chance with partial loops unrolling
///
/// Modified by Nick Kurshev <nickols_k@mail.ru>
///
/ synth_1to1_3dnow works the same way as the c version of
/ synth_1to1. this assembler code based 'decode-i586.s'
/ (by Stefan Bieschewski <stb@acm.org>), two types of changes
/ have been made:
/ - use {MMX,3DNow!} instruction for reduce cpu
/ - remove unused(?) local symbols
/
/ useful sources of information on optimizing 3DNow! code include:
/ AMD 3DNow! Technology Manual (Publication #21928)
/     English:  http://www.amd.com/K6/k6docs/pdf/21928d.pdf
/    (Japanese: http://www.amd.com/japan/K6/k6docs/j21928c.pdf)
/ AMD-K6-2 Processor Code Optimization Application Note (Publication #21924)
/     English:  http://www.amd.com/K6/k6docs/pdf/21924b.pdf
/
/ This code was tested only AMD-K6-2 processor Linux systems,
/ please tell me:
/ - whether this code works on other 3DNow! capable processors
/  (ex.IDT-C6-2) or not
/ - whether this code works on other OSes or not
/
/ by KIMURA Takuhiro <kim@hannah.ipc.miyakyo-u.ac.jp> - until 31.Mar.1998
/                    <kim@comtec.co.jp>               - after  1.Apr.1998

/ Enhancments for q-word operation by Michael Hipp

.bss
        .comm   buffs,4352,4
.data
        .align 8
null_one: .long 0x0000ffff, 0x0000ffff
one_null: .long 0xffff0000, 0xffff0000
bo:       .long 1
.text
/* int synth_1to1(real *bandPtr,int channel,unsigned char *out) */
.globl synth_1to1_3dnowex
synth_1to1_3dnowex:
        subl  $12,%esp
        pushl %ebp
        pushl %edi
        pushl %esi
        pushl %ebx
	
        movl  32(%esp),%eax
        movl  40(%esp),%esi
        movl  $0,%edi
        movl  bo,%ebp
        cmpl  %edi,36(%esp)
        jne   .L48
        decl  %ebp
        andl  $15,%ebp
        movl  %ebp,bo
        movl  $buffs,%ecx
        jmp   .L49
.L48:
        addl  $2,%esi
        movl  $buffs+2176,%ecx
.L49:
        testl $1,%ebp
        je    .L50
        movl  %ecx,%ebx
        movl  %ebp,16(%esp)
        pushl %eax
        movl  20(%esp),%edx
        leal  (%ebx,%edx,4),%eax
        pushl %eax
        movl  24(%esp),%eax
        incl  %eax
        andl  $15,%eax
        leal  1088(,%eax,4),%eax
        addl  %ebx,%eax
        jmp   .L74
.L50:
        leal  1088(%ecx),%ebx
        leal  1(%ebp),%edx
        movl  %edx,16(%esp)
        pushl %eax
        leal  1092(%ecx,%ebp,4),%eax
        pushl %eax
        leal  (%ecx,%ebp,4),%eax
.L74:
        pushl %eax
        call  dct64_3dnowex
        movl  16(%esp),%edx
        leal  0(,%edx,4),%edx
        movl  $decwin+64,%eax
        movl  %eax,%ecx            
        subl  %edx,%ecx
        movl  $8,%ebp
	prefetchw (%esi)
.align 16
.L55:

        movq  (%ecx),%mm0
        pfmul (%ebx),%mm0
        movq  128(%ecx),%mm4
        pfmul 64(%ebx),%mm4

        movq  8(%ecx),%mm1
        pfmul 8(%ebx),%mm1
        pfadd %mm1,%mm0
        movq  136(%ecx),%mm5
        pfmul 72(%ebx),%mm5
        pfadd %mm5,%mm4

        movq  16(%ebx),%mm2
        pfmul 16(%ecx),%mm2
        pfadd %mm2,%mm0
        movq  80(%ebx),%mm6
        pfmul 144(%ecx),%mm6
        pfadd %mm6,%mm4

        movq  24(%ecx),%mm3
        pfmul 24(%ebx),%mm3
        pfadd %mm3,%mm0
        movq  152(%ecx),%mm7
        pfmul 88(%ebx),%mm7
        pfadd %mm7,%mm4

        movq  32(%ebx),%mm1
        pfmul 32(%ecx),%mm1
        pfadd %mm1,%mm0
        movq  96(%ebx),%mm5
        pfmul 160(%ecx),%mm5
        pfadd %mm5,%mm4

        movq  40(%ecx),%mm2
        pfmul 40(%ebx),%mm2
	pfadd %mm2,%mm0
        movq  168(%ecx),%mm6
        pfmul 104(%ebx),%mm6
	pfadd %mm6,%mm4

        movq  48(%ebx),%mm3
        pfmul 48(%ecx),%mm3
        pfadd %mm3,%mm0
        movq  112(%ebx),%mm7
        pfmul 176(%ecx),%mm7
        pfadd %mm7,%mm4

        movq  56(%ecx),%mm1
        pfmul 56(%ebx),%mm1
        pfadd %mm1,%mm0
        movq  184(%ecx),%mm5
        pfmul 120(%ebx),%mm5
        pfadd %mm5,%mm4

	pfnacc %mm4, %mm0
	movq   (%esi), %mm1
	pf2id  %mm0, %mm0
	pand   one_null, %mm1
	psrld  $16,%mm0
	pand   null_one, %mm0
	por    %mm0, %mm1
	movq   %mm1,(%esi)
	
        addl  $128,%ebx
        addl  $256,%ecx
        addl  $8,%esi
        decl  %ebp
        jnz  .L55

/ --- end of  loop 1 ---

	prefetchw (%esi)  /* prefetching for writing this block and next loop */

        movd  (%ecx),%mm0
        pfmul (%ebx),%mm0

        movd  8(%ebx),%mm1
        pfmul 8(%ecx),%mm1
        pfadd %mm1,%mm0

        movd  16(%ebx),%mm2
        pfmul 16(%ecx),%mm2
        pfadd %mm2,%mm0

        movd  24(%ebx),%mm3
        pfmul 24(%ecx),%mm3
        pfadd %mm3,%mm0

        movd  32(%ebx),%mm4
        pfmul 32(%ecx),%mm4
        pfadd %mm4,%mm0

        movd  40(%ebx),%mm5
        pfmul 40(%ecx),%mm5
        pfadd %mm5,%mm0

        movd  48(%ebx),%mm6
        pfmul 48(%ecx),%mm6
        pfadd %mm6,%mm0

        movd  56(%ebx),%mm7
        pfmul 56(%ecx),%mm7
        pfadd %mm7,%mm0

        pf2id %mm0,%mm0
        movd  %mm0,%eax

        sar   $16,%eax

        movw  %ax,(%esi)

        subl  $64,%ebx
        addl  $4,%esi
        addl  $256,%ecx
        movl  $7,%ebp
.align 16
.L68:
	pxor  %mm0, %mm0
	pxor  %mm4, %mm4

        movq  (%ecx),%mm1
        pfmul (%ebx),%mm1
        pfsub %mm1,%mm0
        movq  128(%ecx),%mm5
        pfmul -64(%ebx),%mm5
        pfsub %mm5,%mm4

        movq  8(%ecx),%mm2
        pfmul 8(%ebx),%mm2
        pfsub %mm2,%mm0
        movq  136(%ecx),%mm6
        pfmul -56(%ebx),%mm6
        pfsub %mm6,%mm4

        movq  16(%ecx),%mm3
        pfmul 16(%ebx),%mm3
        pfsub %mm3,%mm0
        movq  144(%ecx),%mm7
        pfmul -48(%ebx),%mm7
        pfsub %mm7,%mm4

        movq  24(%ecx),%mm1
        pfmul 24(%ebx),%mm1
        pfsub %mm1,%mm0
        movq  152(%ecx),%mm5
        pfmul -40(%ebx),%mm5
        pfsub %mm5,%mm4

        movq  32(%ecx),%mm2
        pfmul 32(%ebx),%mm2
        pfsub %mm2,%mm0
        movq  160(%ecx),%mm6
        pfmul -32(%ebx),%mm6
        pfsub %mm6,%mm4

        movq  40(%ecx),%mm3
        pfmul 40(%ebx),%mm3
        pfsub %mm3,%mm0
        movq  168(%ecx),%mm7
        pfmul -24(%ebx),%mm7
        pfsub %mm7,%mm4

        movq  48(%ecx),%mm1
        pfmul 48(%ebx),%mm1
        pfsub %mm1,%mm0
        movq  176(%ecx),%mm5
        pfmul -16(%ebx),%mm5
        pfsub %mm5,%mm4

        movq  56(%ecx),%mm2
        pfmul 56(%ebx),%mm2
        pfsub %mm2,%mm0
        movq  184(%ecx),%mm6
        pfmul -8(%ebx),%mm6
        pfsub %mm6,%mm4

        pfacc  %mm4,%mm0
	movq   (%esi), %mm1
	pf2id  %mm0, %mm0
	pand   one_null, %mm1
	psrld  $16,%mm0
	pand   null_one, %mm0
	por    %mm0, %mm1
	movq   %mm1,(%esi)

        subl  $128,%ebx
        addl  $256,%ecx
        addl  $8,%esi
        decl  %ebp
        jnz   .L68

/ --- end of loop 2

	pxor  %mm0, %mm0

        movq  (%ecx),%mm1
        pfmul (%ebx),%mm1
        pfsub %mm1,%mm0

        movq  8(%ecx),%mm2
        pfmul 8(%ebx),%mm2
        pfsub %mm2,%mm0

        movq  16(%ecx),%mm3
        pfmul 16(%ebx),%mm3
        pfsub %mm3,%mm0

        movq  24(%ecx),%mm4
        pfmul 24(%ebx),%mm4
        pfsub %mm4,%mm0

        movq  32(%ecx),%mm5
        pfmul 32(%ebx),%mm5
        pfsub %mm5,%mm0

        movq  40(%ecx),%mm6
        pfmul 40(%ebx),%mm6
        pfsub %mm6,%mm0

        movq  48(%ecx),%mm7
        pfmul 48(%ebx),%mm7
        pfsub %mm7,%mm0

        movq  56(%ecx),%mm1
        pfmul 56(%ebx),%mm1
        pfsub %mm1,%mm0

        pfacc %mm0,%mm0

        pf2id %mm0,%mm0
        movd  %mm0,%eax

        sar   $16,%eax

        movw  %ax,(%esi)

        femms

        movl  %edi,%eax
        popl  %ebx
        popl  %esi
        popl  %edi
        popl  %ebp
        addl  $12,%esp
        ret
