# This code was taken from http://www.mpg123.org
# See ChangeLog of mpg123-0.59s-pre.1 for detail
# Applied to mplayer by Nick Kurshev <nickols_k@mail.ru>
# Partial 3dnowex-DSP! optimization by Nick Kurshev
#
# TODO: finish 3dnow! optimization at least in scalar mode
#

.data
	.align 8
plus_minus_3dnow: .long 0x00000000, 0x80000000
costab:
	.long 1056974725
	.long 1057056395
	.long 1057223771
	.long 1057485416
	.long 1057855544
	.long 1058356026
	.long 1059019886
	.long 1059897405
	.long 1061067246
	.long 1062657950
	.long 1064892987
	.long 1066774581
	.long 1069414683
	.long 1073984175
	.long 1079645762
	.long 1092815430
	.long 1057005197
	.long 1057342072
	.long 1058087743
	.long 1059427869
	.long 1061799040
	.long 1065862217
	.long 1071413542
	.long 1084439708
	.long 1057128951
	.long 1058664893
	.long 1063675095
	.long 1076102863
	.long 1057655764
	.long 1067924853
	.long 1060439283

.text

	.align 16

.globl dct64_MMX_3dnowex
dct64_MMX_3dnowex:
	pushl %ebx
	pushl %esi
	pushl %edi
	subl $256,%esp
	movl 280(%esp),%eax

	leal 128(%esp),%edx
	movl 272(%esp),%esi
	movl 276(%esp),%edi
	movl $costab,%ebx
	orl %ecx,%ecx
	movl %esp,%ecx
	femms	
/* Phase 1*/
	movq	(%eax), %mm0
	movq	8(%eax), %mm4
	movq	%mm0, %mm3
	movq	%mm4, %mm7
	movq	120(%eax), %mm1
	movq	112(%eax), %mm5
	pswapd	%mm1, %mm1
	pswapd	%mm5, %mm5
	pfadd	%mm1, %mm0
	pfadd	%mm5, %mm4
	movq	%mm0, (%edx)
	movq	%mm4, 8(%edx)
	pfsub	%mm1, %mm3
	pfsub	%mm5, %mm7
	pfmul	(%ebx), %mm3
	pfmul	8(%ebx), %mm7
	pswapd	%mm3, %mm3
	pswapd	%mm7, %mm7
	movq	%mm3, 120(%edx)
	movq	%mm7, 112(%edx)

	movq	16(%eax), %mm0
	movq	24(%eax), %mm4
	movq	%mm0, %mm3
	movq	%mm4, %mm7
	movq	104(%eax), %mm1
	movq	96(%eax), %mm5
	pswapd	%mm1, %mm1
	pswapd	%mm5, %mm5
	pfadd	%mm1, %mm0
	pfadd	%mm5, %mm4
	movq	%mm0, 16(%edx)
	movq	%mm4, 24(%edx)
	pfsub	%mm1, %mm3
	pfsub	%mm5, %mm7
	pfmul	16(%ebx), %mm3
	pfmul	24(%ebx), %mm7
	pswapd	%mm3, %mm3
	pswapd	%mm7, %mm7
	movq	%mm3, 104(%edx)
	movq	%mm7, 96(%edx)

	movq	32(%eax), %mm0
	movq	40(%eax), %mm4
	movq	%mm0, %mm3
	movq	%mm4, %mm7
	movq	88(%eax), %mm1
	movq	80(%eax), %mm5
	pswapd	%mm1, %mm1
	pswapd	%mm5, %mm5
	pfadd	%mm1, %mm0
	pfadd	%mm5, %mm4
	movq	%mm0, 32(%edx)
	movq	%mm4, 40(%edx)
	pfsub	%mm1, %mm3
	pfsub	%mm5, %mm7
	pfmul	32(%ebx), %mm3
	pfmul	40(%ebx), %mm7
	pswapd	%mm3, %mm3
	pswapd	%mm7, %mm7
	movq	%mm3, 88(%edx)
	movq	%mm7, 80(%edx)

	movq	48(%eax), %mm0
	movq	56(%eax), %mm4
	movq	%mm0, %mm3
	movq	%mm4, %mm7
	movq	72(%eax), %mm1
	movq	64(%eax), %mm5
	pswapd	%mm1, %mm1
	pswapd	%mm5, %mm5
	pfadd	%mm1, %mm0
	pfadd	%mm5, %mm4
	movq	%mm0, 48(%edx)
	movq	%mm4, 56(%edx)
	pfsub	%mm1, %mm3
	pfsub	%mm5, %mm7
	pfmul	48(%ebx), %mm3
	pfmul	56(%ebx), %mm7
	pswapd	%mm3, %mm3
	pswapd	%mm7, %mm7
	movq	%mm3, 72(%edx)
	movq	%mm7, 64(%edx)

/* Phase 2*/

	movq	(%edx), %mm0
	movq	8(%edx), %mm4
	movq	%mm0, %mm3
	movq	%mm4, %mm7
	movq	56(%edx), %mm1
	movq	48(%edx), %mm5
	pswapd	%mm1, %mm1
	pswapd	%mm5, %mm5
	pfadd	%mm1, %mm0
	pfadd	%mm5, %mm4
	movq	%mm0, (%ecx)
	movq	%mm4, 8(%ecx)
	pfsub	%mm1, %mm3
	pfsub	%mm5, %mm7
	pfmul	64(%ebx), %mm3
	pfmul	72(%ebx), %mm7
	pswapd	%mm3, %mm3
	pswapd	%mm7, %mm7
	movq	%mm3, 56(%ecx)
	movq	%mm7, 48(%ecx)
	
	movq	16(%edx), %mm0
	movq	24(%edx), %mm4
	movq	%mm0, %mm3
	movq	%mm4, %mm7
	movq	40(%edx), %mm1
	movq	32(%edx), %mm5
	pswapd	%mm1, %mm1
	pswapd	%mm5, %mm5
	pfadd	%mm1, %mm0
	pfadd	%mm5, %mm4
	movq	%mm0, 16(%ecx)
	movq	%mm4, 24(%ecx)
	pfsub	%mm1, %mm3
	pfsub	%mm5, %mm7
	pfmul	80(%ebx), %mm3
	pfmul	88(%ebx), %mm7
	pswapd	%mm3, %mm3
	pswapd	%mm7, %mm7
	movq	%mm3, 40(%ecx)
	movq	%mm7, 32(%ecx)

/* Phase 3*/

	movq	64(%edx), %mm0
	movq	72(%edx), %mm4
	movq	%mm0, %mm3
	movq	%mm4, %mm7
	movq	120(%edx), %mm1
	movq	112(%edx), %mm5
	pswapd	%mm1, %mm1
	pswapd	%mm5, %mm5
	pfadd	%mm1, %mm0
	pfadd	%mm5, %mm4
	movq	%mm0, 64(%ecx)
	movq	%mm4, 72(%ecx)
	pfsubr	%mm1, %mm3
	pfsubr	%mm5, %mm7
	pfmul	64(%ebx), %mm3
	pfmul	72(%ebx), %mm7
	pswapd	%mm3, %mm3
	pswapd	%mm7, %mm7
	movq	%mm3, 120(%ecx)
	movq	%mm7, 112(%ecx)

	movq	80(%edx), %mm0
	movq	88(%edx), %mm4
	movq	%mm0, %mm3
	movq	%mm4, %mm7
	movq	104(%edx), %mm1
	movq	96(%edx), %mm5
	pswapd	%mm1, %mm1
	pswapd	%mm5, %mm5
	pfadd	%mm1, %mm0
	pfadd	%mm5, %mm4
	movq	%mm0, 80(%ecx)
	movq	%mm4, 88(%ecx)
	pfsubr	%mm1, %mm3
	pfsubr	%mm5, %mm7
	pfmul	80(%ebx), %mm3
	pfmul	88(%ebx), %mm7
	pswapd	%mm3, %mm3
	pswapd	%mm7, %mm7
	movq	%mm3, 104(%ecx)
	movq	%mm7, 96(%ecx)
	
/* Phase 4*/

	movq	(%ecx), %mm0
	movq	8(%ecx), %mm4
	movq	%mm0, %mm3
	movq	%mm4, %mm7
	movq	24(%ecx), %mm1
	movq	16(%ecx), %mm5
	pswapd	%mm1, %mm1
	pswapd	%mm5, %mm5
	pfadd	%mm1, %mm0
	pfadd	%mm5, %mm4
	movq	%mm0, (%edx)
	movq	%mm4, 8(%edx)
	pfsub	%mm1, %mm3
	pfsub	%mm5, %mm7
	pfmul	96(%ebx), %mm3
	pfmul	104(%ebx), %mm7
	pswapd	%mm3, %mm3
	pswapd	%mm7, %mm7
	movq	%mm3, 24(%edx)
	movq	%mm7, 16(%edx)

	movq	32(%ecx), %mm0
	movq	40(%ecx), %mm4
	movq	%mm0, %mm3
	movq	%mm4, %mm7
	movq	56(%ecx), %mm1
	movq	48(%ecx), %mm5
	pswapd	%mm1, %mm1
	pswapd	%mm5, %mm5
	pfadd	%mm1, %mm0
	pfadd	%mm5, %mm4
	movq	%mm0, 32(%edx)
	movq	%mm4, 40(%edx)
	pfsubr	%mm1, %mm3
	pfsubr	%mm5, %mm7
	pfmul	96(%ebx), %mm3
	pfmul	104(%ebx), %mm7
	pswapd	%mm3, %mm3
	pswapd	%mm7, %mm7
	movq	%mm3, 56(%edx)
	movq	%mm7, 48(%edx)

	movq	64(%ecx), %mm0
	movq	72(%ecx), %mm4
	movq	%mm0, %mm3
	movq	%mm4, %mm7
	movq	88(%ecx), %mm1
	movq	80(%ecx), %mm5
	pswapd	%mm1, %mm1
	pswapd	%mm5, %mm5
	pfadd	%mm1, %mm0
	pfadd	%mm5, %mm4
	movq	%mm0, 64(%edx)
	movq	%mm4, 72(%edx)
	pfsub	%mm1, %mm3
	pfsub	%mm5, %mm7
	pfmul	96(%ebx), %mm3
	pfmul	104(%ebx), %mm7
	pswapd	%mm3, %mm3
	pswapd	%mm7, %mm7
	movq	%mm3, 88(%edx)
	movq	%mm7, 80(%edx)

	movq	96(%ecx), %mm0
	movq	104(%ecx), %mm4
	movq	%mm0, %mm3
	movq	%mm4, %mm7
	movq	120(%ecx), %mm1
	movq	112(%ecx), %mm5
	pswapd	%mm1, %mm1
	pswapd	%mm5, %mm5
	pfadd	%mm1, %mm0
	pfadd	%mm5, %mm4
	movq	%mm0, 96(%edx)
	movq	%mm4, 104(%edx)
	pfsubr	%mm1, %mm3
	pfsubr	%mm5, %mm7
	pfmul	96(%ebx), %mm3
	pfmul	104(%ebx), %mm7
	pswapd	%mm3, %mm3
	pswapd	%mm7, %mm7
	movq	%mm3, 120(%edx)
	movq	%mm7, 112(%edx)

/* Phase 5 */

	movq	(%edx), %mm0
	movq	16(%edx), %mm4
	movq	%mm0, %mm3
	movq	%mm4, %mm7
	movq	8(%edx), %mm1
	movq	24(%edx), %mm5
	pswapd	%mm1, %mm1
	pswapd	%mm5, %mm5
	pfadd	%mm1, %mm0
	pfadd	%mm5, %mm4
	movq	%mm0, (%ecx)
	movq	%mm4, 16(%ecx)
	pfsub	%mm1, %mm3
	pfsubr	%mm5, %mm7
	pfmul	112(%ebx), %mm3
	pfmul	112(%ebx), %mm7
	pswapd	%mm3, %mm3
	pswapd	%mm7, %mm7
	movq	%mm3, 8(%ecx)
	movq	%mm7, 24(%ecx)

	movq	32(%edx), %mm0
	movq	48(%edx), %mm4
	movq	%mm0, %mm3
	movq	%mm4, %mm7
	movq	40(%edx), %mm1
	movq	56(%edx), %mm5
	pswapd	%mm1, %mm1
	pswapd	%mm5, %mm5
	pfadd	%mm1, %mm0
	pfadd	%mm5, %mm4
	movq	%mm0, 32(%ecx)
	movq	%mm4, 48(%ecx)
	pfsub	%mm1, %mm3
	pfsubr	%mm5, %mm7
	pfmul	112(%ebx), %mm3
	pfmul	112(%ebx), %mm7
	pswapd	%mm3, %mm3
	pswapd	%mm7, %mm7
	movq	%mm3, 40(%ecx)
	movq	%mm7, 56(%ecx)

	movq	64(%edx), %mm0
	movq	80(%edx), %mm4
	movq	%mm0, %mm3
	movq	%mm4, %mm7
	movq	72(%edx), %mm1
	movq	88(%edx), %mm5
	pswapd	%mm1, %mm1
	pswapd	%mm5, %mm5
	pfadd	%mm1, %mm0
	pfadd	%mm5, %mm4
	movq	%mm0, 64(%ecx)
	movq	%mm4, 80(%ecx)
	pfsub	%mm1, %mm3
	pfsubr	%mm5, %mm7
	pfmul	112(%ebx), %mm3
	pfmul	112(%ebx), %mm7
	pswapd	%mm3, %mm3
	pswapd	%mm7, %mm7
	movq	%mm3, 72(%ecx)
	movq	%mm7, 88(%ecx)

	movq	96(%edx), %mm0
	movq	112(%edx), %mm4
	movq	%mm0, %mm3
	movq	%mm4, %mm7
	movq	104(%edx), %mm1
	movq	120(%edx), %mm5
	pswapd	%mm1, %mm1
	pswapd	%mm5, %mm5
	pfadd	%mm1, %mm0
	pfadd	%mm5, %mm4
	movq	%mm0, 96(%ecx)
	movq	%mm4, 112(%ecx)
	pfsub	%mm1, %mm3
	pfsubr	%mm5, %mm7
	pfmul	112(%ebx), %mm3
	pfmul	112(%ebx), %mm7
	pswapd	%mm3, %mm3
	pswapd	%mm7, %mm7
	movq	%mm3, 104(%ecx)
	movq	%mm7, 120(%ecx)
	
/* Phase 6. This is the end of easy road. */
	movl	$1, %eax
	movd	%eax, %mm7
	pi2fd	%mm7, %mm7
	movq	32(%ecx), %mm0
	punpckldq 120(%ebx), %mm7 /* 1.0 | 120(%ebx) */	
	movq	%mm0, %mm1
	movq	plus_minus_3dnow, %mm6
	/* n.b.: pfpnacc */
	pxor	%mm6, %mm1
	pfacc	%mm1, %mm0
	/**/
	pfmul	%mm7, %mm0
	movq	%mm0, 32(%edx)
	femms

	flds   44(%ecx)
	fsubs  40(%ecx)
	fmuls 120(%ebx)

	fsts   44(%edx)
	fadds  40(%ecx) /* pfacc 40(ecx), 56(%ecx) */
	fadds  44(%ecx)
	fstps  40(%edx)

	flds   48(%ecx)
	fsubs  52(%ecx)
	fmuls 120(%ebx)

	flds   60(%ecx)
	fsubs  56(%ecx)
	fmuls 120(%ebx)

	fld      %st(0)
	fadds  56(%ecx)
	fadds  60(%ecx)

	fld      %st(0)
	fadds  48(%ecx)
	fadds  52(%ecx)
	fstps  48(%edx)
	fadd     %st(2)
	fstps  56(%edx)
	fsts   60(%edx)
	faddp    %st(1)
	fstps  52(%edx)
/*---*/
	flds   64(%ecx)
	fadds  68(%ecx)
	fstps  64(%edx)

	flds   64(%ecx)
	fsubs  68(%ecx)
	fmuls 120(%ebx)
	fstps  68(%edx)

	flds   76(%ecx)
	fsubs  72(%ecx)
	fmuls 120(%ebx)
	fsts   76(%edx)
	fadds  72(%ecx)
	fadds  76(%ecx)
	fstps  72(%edx)

	flds   92(%ecx)
	fsubs  88(%ecx)
	fmuls 120(%ebx)
	fsts   92(%edx)
	fadds  92(%ecx)
	fadds  88(%ecx)

	fld      %st(0)
	fadds  80(%ecx)
	fadds  84(%ecx)
	fstps  80(%edx)

	flds   80(%ecx)
	fsubs  84(%ecx)
	fmuls 120(%ebx)
	fadd  %st(0), %st(1)
	fadds 92(%edx)
	fstps 84(%edx)
	fstps 88(%edx)

	flds   96(%ecx)
	fadds 100(%ecx)
	fstps  96(%edx)

	flds   96(%ecx)
	fsubs 100(%ecx)
	fmuls 120(%ebx)
	fstps 100(%edx)

	flds  108(%ecx)
	fsubs 104(%ecx)
	fmuls 120(%ebx)
	fsts  108(%edx)
	fadds 104(%ecx)
	fadds 108(%ecx)
	fstps 104(%edx)

	flds  124(%ecx)
	fsubs 120(%ecx)
	fmuls 120(%ebx)
	fsts  124(%edx)
	fadds 120(%ecx)
	fadds 124(%ecx)

	fld      %st(0)
	fadds 112(%ecx)
	fadds 116(%ecx)
	fstps 112(%edx)

	flds  112(%ecx)
	fsubs 116(%ecx)
	fmuls 120(%ebx)
	fadd  %st(0),%st(1)
	fadds 124(%edx)
	fstps 116(%edx)
	fstps 120(%edx)
	jnz .L01
	
/* Phase 7*/

	flds      (%ecx)
	fadds    4(%ecx)
	fstps 1024(%esi)

	flds      (%ecx)
	fsubs    4(%ecx)
	fmuls  120(%ebx)
	fsts      (%esi)
	fstps     (%edi)

	flds   12(%ecx)
	fsubs   8(%ecx)
	fmuls 120(%ebx)
	fsts  512(%edi)
	fadds  12(%ecx)
	fadds   8(%ecx)
	fstps 512(%esi)

	flds   16(%ecx)
	fsubs  20(%ecx)
	fmuls 120(%ebx)

	flds   28(%ecx)
	fsubs  24(%ecx)
	fmuls 120(%ebx)
	fsts  768(%edi)
	fld      %st(0)
	fadds  24(%ecx)
	fadds  28(%ecx)
	fld      %st(0)
	fadds  16(%ecx)
	fadds  20(%ecx)
	fstps 768(%esi)
	fadd     %st(2)
	fstps 256(%esi)
	faddp    %st(1)
	fstps 256(%edi)
	
/* Phase 8*/

	flds   32(%edx)
	fadds  48(%edx)
	fstps 896(%esi)

	flds   48(%edx)
	fadds  40(%edx)
	fstps 640(%esi)

	flds   40(%edx)
	fadds  56(%edx)
	fstps 384(%esi)

	flds   56(%edx)
	fadds  36(%edx)
	fstps 128(%esi)

	flds   36(%edx)
	fadds  52(%edx)
	fstps 128(%edi)

	flds   52(%edx)
	fadds  44(%edx)
	fstps 384(%edi)

	flds   60(%edx)
	fsts  896(%edi)
	fadds  44(%edx)
	fstps 640(%edi)

	flds   96(%edx)
	fadds 112(%edx)
	fld      %st(0)
	fadds  64(%edx)
	fstps 960(%esi)
	fadds  80(%edx)
	fstps 832(%esi)

	flds  112(%edx)
	fadds 104(%edx)
	fld      %st(0)
	fadds  80(%edx)
	fstps 704(%esi)
	fadds  72(%edx)
	fstps 576(%esi)

	flds  104(%edx)
	fadds 120(%edx)
	fld      %st(0)
	fadds  72(%edx)
	fstps 448(%esi)
	fadds  88(%edx)
	fstps 320(%esi)

	flds  120(%edx)
	fadds 100(%edx)
	fld      %st(0)
	fadds  88(%edx)
	fstps 192(%esi)
	fadds  68(%edx)
	fstps  64(%esi)

	flds  100(%edx)
	fadds 116(%edx)
	fld      %st(0)
	fadds  68(%edx)
	fstps  64(%edi)
	fadds  84(%edx)
	fstps 192(%edi)

	flds  116(%edx)
	fadds 108(%edx)
	fld      %st(0)
	fadds  84(%edx)
	fstps 320(%edi)
	fadds  76(%edx)
	fstps 448(%edi)

	flds  108(%edx)
	fadds 124(%edx)
	fld      %st(0)
	fadds  76(%edx)
	fstps 576(%edi)
	fadds  92(%edx)
	fstps 704(%edi)

	flds  124(%edx)
	fsts  960(%edi)
	fadds  92(%edx)
	fstps 832(%edi)
	jmp	.L_bye
.L01:	
/* Phase 9*/

	flds      (%ecx)
	fadds    4(%ecx)
	fistp  512(%esi)

	flds      (%ecx)
	fsubs    4(%ecx)
	fmuls  120(%ebx)

	fistp     (%esi)


	flds    12(%ecx)
	fsubs    8(%ecx)
	fmuls  120(%ebx)
	fist   256(%edi)
	fadds   12(%ecx)
	fadds    8(%ecx)
	fistp  256(%esi)

	flds   16(%ecx)
	fsubs  20(%ecx)
	fmuls 120(%ebx)

	flds   28(%ecx)
	fsubs  24(%ecx)
	fmuls 120(%ebx)
	fist  384(%edi)
	fld      %st(0)
	fadds  24(%ecx)
	fadds  28(%ecx)
	fld      %st(0)
	fadds  16(%ecx)
	fadds  20(%ecx)
	fistp  384(%esi)
	fadd     %st(2)
	fistp  128(%esi)
	faddp    %st(1)
	fistp  128(%edi)
	
/* Phase 10*/

	flds    32(%edx)
	fadds   48(%edx)
	fistp  448(%esi)

	flds   48(%edx)
	fadds  40(%edx)
	fistp 320(%esi)

	flds   40(%edx)
	fadds  56(%edx)
	fistp 192(%esi)

	flds   56(%edx)
	fadds  36(%edx)
	fistp  64(%esi)

	flds   36(%edx)
	fadds  52(%edx)
	fistp  64(%edi)

	flds   52(%edx)
	fadds  44(%edx)
	fistp 192(%edi)

	flds   60(%edx)
	fist   448(%edi)
	fadds  44(%edx)
	fistp 320(%edi)

	flds   96(%edx)
	fadds 112(%edx)
	fld      %st(0)
	fadds  64(%edx)
	fistp 480(%esi)
	fadds  80(%edx)
	fistp 416(%esi)

	flds  112(%edx)
	fadds 104(%edx)
	fld      %st(0)
	fadds  80(%edx)
	fistp 352(%esi)
	fadds  72(%edx)
	fistp 288(%esi)

	flds  104(%edx)
	fadds 120(%edx)
	fld      %st(0)
	fadds  72(%edx)
	fistp 224(%esi)
	fadds  88(%edx)
	fistp 160(%esi)

	flds  120(%edx)
	fadds 100(%edx)
	fld      %st(0)
	fadds  88(%edx)
	fistp  96(%esi)
	fadds  68(%edx)
	fistp  32(%esi)

	flds  100(%edx)
	fadds 116(%edx)
	fld      %st(0)
	fadds  68(%edx)
	fistp  32(%edi)
	fadds  84(%edx)
	fistp  96(%edi)

	flds  116(%edx)
	fadds 108(%edx)
	fld      %st(0)
	fadds  84(%edx)
	fistp 160(%edi)
	fadds  76(%edx)
	fistp 224(%edi)

	flds  108(%edx)
	fadds 124(%edx)
	fld      %st(0)
	fadds  76(%edx)
	fistp 288(%edi)
	fadds  92(%edx)
	fistp 352(%edi)

	flds  124(%edx)
	fist  480(%edi)
	fadds  92(%edx)
	fistp 416(%edi)
	movsw
.L_bye:
	addl $256,%esp
	popl %edi
	popl %esi
	popl %ebx
	ret
	

