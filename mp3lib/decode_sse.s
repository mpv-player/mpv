///
/// Replacement of synth_1to1() with Intel's SSE SIMD operations support
///
/// This code based 'decode_k7.s' by Nick Kurshev
/// <squash@mb.kcom.ne.jp>,only some types of changes have been made:
///
///  - SSE optimization
///  - change function name for support SSE automatic detect
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
        .align 4
bo:
        .long 1
.text
/* int synth_1to1(real *bandPtr,int channel,unsigned char *out) */
.globl synth_1to1_sse
synth_1to1_sse:
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
        call  dct64
	addl  $12, %esp
        movl  16(%esp),%edx
        leal  0(,%edx,4),%edx
        movl  $decwin+64,%eax
        movl  %eax,%ecx            
        subl  %edx,%ecx
        movl  $16,%ebp

.L55:
	movups (%ecx), %xmm0
	mulps  (%ebx), %xmm0
	movups 16(%ecx), %xmm1
	mulps  16(%ebx), %xmm1
	addps  %xmm1, %xmm0
	movups 32(%ecx), %xmm1
	mulps  32(%ebx), %xmm1
	addps  %xmm1, %xmm0
	movups 48(%ecx), %xmm1
	mulps  48(%ebx), %xmm1
	addps  %xmm1, %xmm0
	movhlps %xmm0, %xmm1
	addps   %xmm1, %xmm0
	movaps  %xmm0, %xmm1
	shufps  $0x55, %xmm1, %xmm1 /* fake of pfnacc. 1|1|1|1 */
	subss	%xmm1, %xmm0
	cvttss2si %xmm0, %eax

/        sar   $16,%eax
        movw  %ax,(%esi)

        addl  $64,%ebx
        subl  $-128,%ecx
        addl  $4,%esi
        decl  %ebp
        jnz  .L55

/ --- end of  loop 1 ---

	movups (%ecx), %xmm0
	mulps  (%ebx), %xmm0
	movups 16(%ecx), %xmm1
	mulps  16(%ebx), %xmm1
	addps  %xmm1, %xmm0
	movups 32(%ecx), %xmm1
	mulps  32(%ebx), %xmm1
	addps  %xmm1, %xmm0
	movups 48(%ecx), %xmm1
	mulps  48(%ebx), %xmm1
	addps  %xmm1, %xmm0
	movhlps %xmm0, %xmm1	
	addss	%xmm1, %xmm0
	cvttss2si %xmm0, %eax

/        sar   $16,%eax

        movw  %ax,(%esi)

        addl  $-64,%ebx
        addl  $4,%esi
        addl  $256,%ecx
        movl  $15,%ebp

.L68:
	xorps %xmm0, %xmm0
	movups (%ecx), %xmm1
	mulps  (%ebx), %xmm1
	subps  %xmm1, %xmm0
	movups 16(%ecx), %xmm1
	mulps  16(%ebx), %xmm1
	subps  %xmm1, %xmm0
	movups 32(%ecx), %xmm1
	mulps  32(%ebx), %xmm1
	subps  %xmm1, %xmm0
	movups 48(%ecx), %xmm1
	mulps  48(%ebx), %xmm1
	subps  %xmm1, %xmm0
	movhlps %xmm0, %xmm1
	subps	%xmm1, %xmm0
	movaps	%xmm0, %xmm1
	shufps $0x55, %xmm1, %xmm1 /* fake of pfacc 1|1|1|1 */
	addss  %xmm1, %xmm0
	cvttss2si %xmm0, %eax

/        sar   $16,%eax

        movw  %ax,(%esi)

        addl  $-64,%ebx
        subl  $-128,%ecx
        addl  $4,%esi
        decl  %ebp
        jnz   .L68

/ --- end of loop 2

        movl  %edi,%eax
        popl  %ebx
        popl  %esi
        popl  %edi
        popl  %ebp
        addl  $12,%esp
        ret
