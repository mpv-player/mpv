# This code is a translation of dct64_k7.s from MPlayer.
# Coded by Felix Buenemann <atmosfear at users.sourceforge.net>
#
# TODO: - fix phases 4 and 5 (sse)
#       - optimize scalar FPU code? (interleave with sse code)
#

//.data
//	.align 8
//x_plus_minus_3dnow: .long 0x00000000, 0x80000000
//plus_1f: .float 1.0

.text

	.align 16

	.global dct64_MMX_sse

dct64_MMX_sse:
	pushl %ebx
	pushl %esi
	pushl %edi
	subl $256,%esp
	movl 280(%esp),%eax

	leal 128(%esp),%edx
	movl 272(%esp),%esi
	movl 276(%esp),%edi
	movl $costab_mmx,%ebx
	orl %ecx,%ecx
	movl %esp,%ecx

/* Phase 1 (complete, worx) */

// [1] Process Block A1 (16 Bytes)
/	movq	(%eax), %mm0
/	movq	8(%eax), %mm4
	movups	(%eax), %xmm0

// Copy A1 to another register A2
/	movq	%mm0, %mm3
/	movq	%mm4, %mm7
	movaps	%xmm0, %xmm2

// Process Block B1 (last 16 bytes)
/	movq	120(%eax), %mm1
/	movq	112(%eax), %mm5
	movups	112(%eax), %xmm1

/* The PSWAPD instruction swaps or reverses the upper and lower
 * doublewords of the source operand.  PSWAPD mmreg1, mmreg2
 * performs the following operations:
 * temp = mmreg2
 * mmreg1[63:32] = temp[31:0 ]
 * mmreg1[31:0 ] = temp[63:32]
 */
/	pswapd	%mm1, %mm1
/	pswapd	%mm5, %mm5
// shufps here exchanges a,b,c,d to b,a,d,c in xmm1 (desc ia32-ref p.752)
////	shufps	$177, %xmm1, %xmm1
	shufps	$27, %xmm1, %xmm1

// Add B1 to A1
/	pfadd	%mm1, %mm0
/	pfadd	%mm5, %mm4
	addps	%xmm1, %xmm0

// Save Block A1 
/	movq	%mm0, (%edx)
/	movq	%mm4, 8(%edx)
	movups	%xmm0, (%edx)

// Sub B1 from A2
/	pfsub	%mm1, %mm3
/	pfsub	%mm5, %mm7
	subps	%xmm1, %xmm2

// Mul mem with A2
/	pfmul	(%ebx), %mm3
/	pfmul	8(%ebx), %mm7
	movups	(%ebx), %xmm7
	mulps	%xmm7, %xmm2

// Shuffle A2
/	pswapd	%mm3, %mm3
/	pswapd	%mm7, %mm7
// I do a,b,c,d -> d,c,b,a to suit order when writing to mem (saves one shufps)
	shufps	$27, %xmm2, %xmm2

// Save A2 to mem (end)
/	movq	%mm3, 120(%edx)
/	movq	%mm7, 112(%edx)
	movups	%xmm2, 112(%edx)

// [2] Process next data block
/	movq	16(%eax), %mm0
/	movq	24(%eax), %mm4
	movups	16(%eax), %xmm0

/	movq	%mm0, %mm3
/	movq	%mm4, %mm7
	movaps	%xmm0, %xmm2

/	movq	104(%eax), %mm1
/	movq	96(%eax), %mm5
	movups	96(%eax), %xmm1

/	pswapd	%mm1, %mm1
/	pswapd	%mm5, %mm5
////	shufps	$177, %xmm1, %xmm1
	shufps	$27, %xmm1, %xmm1

/	pfadd	%mm1, %mm0
/	pfadd	%mm5, %mm4
	addps	%xmm1, %xmm0

/	movq	%mm0, 16(%edx)
/	movq	%mm4, 24(%edx)
	movups	%xmm0, 16(%edx)

/	pfsub	%mm1, %mm3
/	pfsub	%mm5, %mm7
	subps	%xmm1, %xmm2

/	pfmul	16(%ebx), %mm3
/	pfmul	24(%ebx), %mm7
	movups	16(%ebx), %xmm7
	mulps	%xmm7, %xmm2

/	pswapd	%mm3, %mm3
/	pswapd	%mm7, %mm7
	shufps $27, %xmm2, %xmm2

/	movq	%mm3, 104(%edx)
/	movq	%mm7, 96(%edx)
	movups	%xmm2, 96(%edx)

// [3]
/	movq	32(%eax), %mm0
/	movq	40(%eax), %mm4
	movups	32(%eax), %xmm0
	
/	movq	%mm0, %mm3
/	movq	%mm4, %mm7
	movaps	%xmm0, %xmm2

/	movq	88(%eax), %mm1
/	movq	80(%eax), %mm5
	movups	80(%eax), %xmm1

/	pswapd	%mm1, %mm1
/	pswapd	%mm5, %mm5
////	shufps	$177, %xmm1, %xmm1
	shufps	$27, %xmm1, %xmm1

/	pfadd	%mm1, %mm0
/	pfadd	%mm5, %mm4
	addps	%xmm1, %xmm0

/	movq	%mm0, 32(%edx)
/	movq	%mm4, 40(%edx)
	movups	%xmm0, 32(%edx)

/	pfsub	%mm1, %mm3
/	pfsub	%mm5, %mm7
	subps	%xmm1, %xmm2

/	pfmul	32(%ebx), %mm3
/	pfmul	40(%ebx), %mm7
	movups	32(%ebx), %xmm7
	mulps	%xmm7, %xmm2

/	pswapd	%mm3, %mm3
/	pswapd	%mm7, %mm7
	shufps	$27, %xmm2, %xmm2

/	movq	%mm3, 88(%edx)
/	movq	%mm7, 80(%edx)
	movups	%xmm2, 80(%edx)

// [4]
/	movq	48(%eax), %mm0
/	movq	56(%eax), %mm4
	movups	48(%eax), %xmm0

/	movq	%mm0, %mm3
/	movq	%mm4, %mm7
	movaps	%xmm0, %xmm2

/	movq	72(%eax), %mm1
/	movq	64(%eax), %mm5
	movups	64(%eax), %xmm1

/	pswapd	%mm1, %mm1
/	pswapd	%mm5, %mm5
////	shufps	$177, %xmm1, %xmm1
	shufps	$27, %xmm1, %xmm1

/	pfadd	%mm1, %mm0
/	pfadd	%mm5, %mm4
	addps	%xmm1, %xmm0

/	movq	%mm0, 48(%edx)
/	movq	%mm4, 56(%edx)
	movups	%xmm0, 48(%edx)

/	pfsub	%mm1, %mm3
/	pfsub	%mm5, %mm7
	subps	%xmm1, %xmm2

/	pfmul	48(%ebx), %mm3
/	pfmul	56(%ebx), %mm7
	movups	48(%ebx), %xmm7
	mulps	%xmm7, %xmm2

/	pswapd	%mm3, %mm3
/	pswapd	%mm7, %mm7
	shufps	$27, %xmm2, %xmm2

/	movq	%mm3, 72(%edx)
/	movq	%mm7, 64(%edx)
	movups	%xmm2, 64(%edx)


// phase 1 fpu code
/* Phase 1*/
/*
	flds     (%eax)
	leal 128(%esp),%edx
	fadds 124(%eax)
	movl 272(%esp),%esi
	fstps    (%edx)
	movl 276(%esp),%edi

	flds    4(%eax)
	movl $costab_mmx,%ebx
	fadds 120(%eax)
	orl %ecx,%ecx
	fstps   4(%edx)

	flds     (%eax)
	movl %esp,%ecx
	fsubs 124(%eax)
	fmuls    (%ebx)
	fstps 124(%edx)

	flds    4(%eax)
	fsubs 120(%eax)
	fmuls   4(%ebx)
	fstps 120(%edx)

	flds    8(%eax)
	fadds 116(%eax)
	fstps   8(%edx)

	flds   12(%eax)
	fadds 112(%eax)
	fstps  12(%edx)

	flds    8(%eax)
	fsubs 116(%eax)
	fmuls   8(%ebx)
	fstps 116(%edx)

	flds   12(%eax)
	fsubs 112(%eax)
	fmuls  12(%ebx)
	fstps 112(%edx)

	flds   16(%eax)
	fadds 108(%eax)
	fstps  16(%edx)

	flds   20(%eax)
	fadds 104(%eax)
	fstps  20(%edx)

	flds   16(%eax)
	fsubs 108(%eax)
	fmuls  16(%ebx)
	fstps 108(%edx)

	flds   20(%eax)
	fsubs 104(%eax)
	fmuls  20(%ebx)
	fstps 104(%edx)

	flds   24(%eax)
	fadds 100(%eax)
	fstps  24(%edx)

	flds   28(%eax)
	fadds  96(%eax)
	fstps  28(%edx)

	flds   24(%eax)
	fsubs 100(%eax)
	fmuls  24(%ebx)
	fstps 100(%edx)

	flds   28(%eax)
	fsubs  96(%eax)
	fmuls  28(%ebx)
	fstps  96(%edx)

	flds   32(%eax)
	fadds  92(%eax)
	fstps  32(%edx)

	flds   36(%eax)
	fadds  88(%eax)
	fstps  36(%edx)

	flds   32(%eax)
	fsubs  92(%eax)
	fmuls  32(%ebx)
	fstps  92(%edx)

	flds   36(%eax)
	fsubs  88(%eax)
	fmuls  36(%ebx)
	fstps  88(%edx)

	flds   40(%eax)
	fadds  84(%eax)
	fstps  40(%edx)

	flds   44(%eax)
	fadds  80(%eax)
	fstps  44(%edx)

	flds   40(%eax)
	fsubs  84(%eax)
	fmuls  40(%ebx)
	fstps  84(%edx)

	flds   44(%eax)
	fsubs  80(%eax)
	fmuls  44(%ebx)
	fstps  80(%edx)

	flds   48(%eax)
	fadds  76(%eax)
	fstps  48(%edx)

	flds   52(%eax)
	fadds  72(%eax)
	fstps  52(%edx)

	flds   48(%eax)
	fsubs  76(%eax)
	fmuls  48(%ebx)
	fstps  76(%edx)

	flds   52(%eax)
	fsubs  72(%eax)
	fmuls  52(%ebx)
	fstps  72(%edx)

	flds   56(%eax)
	fadds  68(%eax)
	fstps  56(%edx)

	flds   60(%eax)
	fadds  64(%eax)
	fstps  60(%edx)

	flds   56(%eax)
	fsubs  68(%eax)
	fmuls  56(%ebx)
	fstps  68(%edx)

	flds   60(%eax)
	fsubs  64(%eax)
	fmuls  60(%ebx)
	fstps  64(%edx)
*/	
// end phase 1 fpu code

/* Phase 2 (completed, worx) */

/	movq	(%edx), %mm0
/	movq	8(%edx), %mm4
	movups	(%edx), %xmm0

/	movq	%mm0, %mm3
/	movq	%mm4, %mm7
	movaps	%xmm0, %xmm2

/	movq	56(%edx), %mm1
/	movq	48(%edx), %mm5
	movups	48(%edx), %xmm1

/	pswapd	%mm1, %mm1
/	pswapd	%mm5, %mm5
////	shufps	$177, %xmm1, %xmm1
	shufps	$27, %xmm1, %xmm1

/	pfadd	%mm1, %mm0
/	pfadd	%mm5, %mm4
	addps	%xmm1, %xmm0

/	movq	%mm0, (%ecx)
/	movq	%mm4, 8(%ecx)
	movups	%xmm0, (%ecx)

/	pfsub	%mm1, %mm3
/	pfsub	%mm5, %mm7
	subps	%xmm1, %xmm2

/	pfmul	64(%ebx), %mm3
/	pfmul	72(%ebx), %mm7
	movups	64(%ebx), %xmm7
	mulps	%xmm7, %xmm2

/	pswapd	%mm3, %mm3
/	pswapd	%mm7, %mm7
	shufps	$27, %xmm2, %xmm2

/	movq	%mm3, 56(%ecx)
/	movq	%mm7, 48(%ecx)
	movups	%xmm2, 48(%ecx)
	
/	movq	16(%edx), %mm0
/	movq	24(%edx), %mm4
	movups	16(%edx), %xmm0

/	movq	%mm0, %mm3
/	movq	%mm4, %mm7
	movaps	%xmm0, %xmm2

/	movq	40(%edx), %mm1
/	movq	32(%edx), %mm5
	movups	32(%edx), %xmm1

/	pswapd	%mm1, %mm1
/	pswapd	%mm5, %mm5
////	shufps	$177, %xmm1, %xmm1
	shufps	$27, %xmm1, %xmm1

/	pfadd	%mm1, %mm0
/	pfadd	%mm5, %mm4
	addps	%xmm1, %xmm0

/	movq	%mm0, 16(%ecx)
/	movq	%mm4, 24(%ecx)
	movups	%xmm0, 16(%ecx)

/	pfsub	%mm1, %mm3
/	pfsub	%mm5, %mm7
	subps	%xmm1, %xmm2

/	pfmul	80(%ebx), %mm3
/	pfmul	88(%ebx), %mm7
	movups	80(%ebx), %xmm7
	mulps	%xmm7, %xmm2

/	pswapd	%mm3, %mm3
/	pswapd	%mm7, %mm7
	shufps	$27, %xmm2, %xmm2

/	movq	%mm3, 40(%ecx)
/	movq	%mm7, 32(%ecx)
	movups	%xmm2, 32(%ecx)


// phase 2 fpu
/* Phase 2*/
/*
	flds     (%edx)
	fadds  60(%edx)
	fstps    (%ecx)

	flds    4(%edx)
	fadds  56(%edx)
	fstps   4(%ecx)

	flds     (%edx)
	fsubs  60(%edx)
	fmuls  64(%ebx)
	fstps  60(%ecx)

	flds    4(%edx)
	fsubs  56(%edx)
	fmuls  68(%ebx)
	fstps  56(%ecx)

	flds    8(%edx)
	fadds  52(%edx)
	fstps   8(%ecx)

	flds   12(%edx)
	fadds  48(%edx)
	fstps  12(%ecx)

	flds    8(%edx)
	fsubs  52(%edx)
	fmuls  72(%ebx)
	fstps  52(%ecx)

	flds   12(%edx)
	fsubs  48(%edx)
	fmuls  76(%ebx)
	fstps  48(%ecx)

	flds   16(%edx)
	fadds  44(%edx)
	fstps  16(%ecx)

	flds   20(%edx)
	fadds  40(%edx)
	fstps  20(%ecx)

	flds   16(%edx)
	fsubs  44(%edx)
	fmuls  80(%ebx)
	fstps  44(%ecx)

	flds   20(%edx)
	fsubs  40(%edx)
	fmuls  84(%ebx)
	fstps  40(%ecx)

	flds   24(%edx)
	fadds  36(%edx)
	fstps  24(%ecx)

	flds   28(%edx)
	fadds  32(%edx)
	fstps  28(%ecx)

	flds   24(%edx)
	fsubs  36(%edx)
	fmuls  88(%ebx)
	fstps  36(%ecx)

	flds   28(%edx)
	fsubs  32(%edx)
	fmuls  92(%ebx)
	fstps  32(%ecx)
*/	
// end phase 2 fpu

/* Phase 3 (completed, working) */

/	movq	64(%edx), %mm0
/	movq	72(%edx), %mm4
	movups	64(%edx), %xmm0

/	movq	%mm0, %mm3
/	movq	%mm4, %mm7
	movaps	%xmm0, %xmm2

/	movq	120(%edx), %mm1
/	movq	112(%edx), %mm5
	movups	112(%edx), %xmm1

/	pswapd	%mm1, %mm1
/	pswapd	%mm5, %mm5
////	shufps	$177, %xmm1, %xmm1
	shufps	$27, %xmm1, %xmm1

/	pfadd	%mm1, %mm0
/	pfadd	%mm5, %mm4
	addps	%xmm1, %xmm0

/	movq	%mm0, 64(%ecx)
/	movq	%mm4, 72(%ecx)
	movups	%xmm0, 64(%ecx)

/	pfsubr	%mm1, %mm3
/	pfsubr	%mm5, %mm7
// optimized (xmm1<->xmm2)
	subps	%xmm2, %xmm1

/	pfmul	64(%ebx), %mm3
/	pfmul	72(%ebx), %mm7
	movups	64(%ebx), %xmm7
	mulps	%xmm7, %xmm1

/	pswapd	%mm3, %mm3
/	pswapd	%mm7, %mm7
	shufps	$27, %xmm1, %xmm1

/	movq	%mm3, 120(%ecx)
/	movq	%mm7, 112(%ecx)
	movups	%xmm1, 112(%ecx)


/	movq	80(%edx), %mm0
/	movq	88(%edx), %mm4
	movups	80(%edx), %xmm0

/	movq	%mm0, %mm3
/	movq	%mm4, %mm7
	movaps	%xmm0, %xmm2

/	movq	104(%edx), %mm1
/	movq	96(%edx), %mm5
	movups	96(%edx), %xmm1

/	pswapd	%mm1, %mm1
/	pswapd	%mm5, %mm5
////	shufps	$177, %xmm1, %xmm1
	shufps	$27, %xmm1, %xmm1

/	pfadd	%mm1, %mm0
/	pfadd	%mm5, %mm4
	addps	%xmm1, %xmm0

/	movq	%mm0, 80(%ecx)
/	movq	%mm4, 88(%ecx)
	movups	%xmm0, 80(%ecx)

/	pfsubr	%mm1, %mm3
/	pfsubr	%mm5, %mm7
// optimized (xmm1<->xmm2)
	subps	%xmm2, %xmm1

/	pfmul	80(%ebx), %mm3
/	pfmul	88(%ebx), %mm7
	movups	80(%ebx), %xmm7
	mulps	%xmm7, %xmm1

/	pswapd	%mm3, %mm3
/	pswapd	%mm7, %mm7
	shufps	$27, %xmm1, %xmm1

/	movq	%mm3, 104(%ecx)
/	movq	%mm7, 96(%ecx)
	movups	%xmm1, 96(%ecx)


// phase 3 fpu
/* Phase 3*/
/*
	flds   64(%edx)
	fadds 124(%edx)
	fstps  64(%ecx)

	flds   68(%edx)
	fadds 120(%edx)
	fstps  68(%ecx)

	flds  124(%edx)
	fsubs  64(%edx)
	fmuls  64(%ebx)
	fstps 124(%ecx)

	flds  120(%edx)
	fsubs  68(%edx)
	fmuls  68(%ebx)
	fstps 120(%ecx)

	flds   72(%edx)
	fadds 116(%edx)
	fstps  72(%ecx)

	flds   76(%edx)
	fadds 112(%edx)
	fstps  76(%ecx)

	flds  116(%edx)
	fsubs  72(%edx)
	fmuls  72(%ebx)
	fstps 116(%ecx)

	flds  112(%edx)
	fsubs  76(%edx)
	fmuls  76(%ebx)
	fstps 112(%ecx)

	flds   80(%edx)
	fadds 108(%edx)
	fstps  80(%ecx)

	flds   84(%edx)
	fadds 104(%edx)
	fstps  84(%ecx)

	flds  108(%edx)
	fsubs  80(%edx)
	fmuls  80(%ebx)
	fstps 108(%ecx)

	flds  104(%edx)
	fsubs  84(%edx)
	fmuls  84(%ebx)
	fstps 104(%ecx)

	flds   88(%edx)
	fadds 100(%edx)
	fstps  88(%ecx)

	flds   92(%edx)
	fadds  96(%edx)
	fstps  92(%ecx)

	flds  100(%edx)
	fsubs  88(%edx)
	fmuls  88(%ebx)
	fstps 100(%ecx)

	flds   96(%edx)
	fsubs  92(%edx)
	fmuls  92(%ebx)
	fstps  96(%ecx)
*/
// end phase 3 fpu

	
/* Phase 4 (completed, buggy) */
/*
/	movq	96(%ebx), %mm2
/	movq	104(%ebx), %mm6
	movups	96(%ebx), %xmm4


/	movq	(%ecx), %mm0
/	movq	8(%ecx), %mm4
	movups	(%ecx), %xmm0

/	movq	%mm0, %mm3
/	movq	%mm4, %mm7
	movaps	%xmm0, %xmm2

/	movq	24(%ecx), %mm1
/	movq	16(%ecx), %mm5
	movups	16(%ecx), %xmm1

/	pswapd	%mm1, %mm1
/	pswapd	%mm5, %mm5
////	shufps	$177, %xmm1, %xmm1
	shufps	$27, %xmm1, %xmm1

/	pfadd	%mm1, %mm0
/	pfadd	%mm5, %mm4
	addps	%xmm1, %xmm0

/	movq	%mm0, (%edx)
/	movq	%mm4, 8(%edx)
	movups	%xmm0, (%edx)

/	pfsub	%mm1, %mm3
/	pfsub	%mm5, %mm7
	subps	%xmm1, %xmm2

/	pfmul	%mm2, %mm3
/	pfmul	%mm6, %mm7
	mulps	%xmm4, %xmm2

/	pswapd	%mm3, %mm3
/	pswapd	%mm7, %mm7
	shufps	$27, %xmm2, %xmm2

/	movq	%mm3, 24(%edx)
/	movq	%mm7, 16(%edx)
	movups	%xmm2, 16(%edx)

/	movq	32(%ecx), %mm0
/	movq	40(%ecx), %mm4
	movups	32(%ecx), %xmm0

/	movq	%mm0, %mm3
/	movq	%mm4, %mm7
	movaps	%xmm0, %xmm2

/	movq	56(%ecx), %mm1
/	movq	48(%ecx), %mm5
	movups	48(%ecx), %xmm1

/	pswapd	%mm1, %mm1
/	pswapd	%mm5, %mm5
////	shufps	$177, %xmm1, %xmm1
	shufps	$27, %xmm1, %xmm1

/	pfadd	%mm1, %mm0
/	pfadd	%mm5, %mm4
	addps	%xmm1, %xmm0

/	movq	%mm0, 32(%edx)
/	movq	%mm4, 40(%edx)
	movups	%xmm0, 32(%edx)

/	pfsubr	%mm1, %mm3
/	pfsubr	%mm5, %mm7
// Luckily we can swap this (xmm1<->xmm2)
	subps	%xmm2, %xmm1

/	pfmul	%mm2, %mm3
/	pfmul	%mm6, %mm7
	mulps	%xmm4, %xmm1

/	pswapd	%mm3, %mm3
/	pswapd	%mm7, %mm7
	shufps	$27, %xmm1, %xmm1

/	movq	%mm3, 56(%edx)
/	movq	%mm7, 48(%edx)
	movups	%xmm1, 48(%edx)


/	movq	64(%ecx), %mm0
/	movq	72(%ecx), %mm4
	movups	64(%ecx), %xmm0

/	movq	%mm0, %mm3
/	movq	%mm4, %mm7
	movaps	%xmm0, %xmm2

/	movq	88(%ecx), %mm1
/	movq	80(%ecx), %mm5
	movups	80(%ecx), %xmm1

/	pswapd	%mm1, %mm1
/	pswapd	%mm5, %mm5
////	shufps	$177, %xmm1, %xmm1
	shufps	$27, %xmm1, %xmm1

/	pfadd	%mm1, %mm0
/	pfadd	%mm5, %mm4
	addps	%xmm1, %xmm0

/	movq	%mm0, 64(%edx)
/	movq	%mm4, 72(%edx)
	movups	%xmm0, 64(%edx)

/	pfsub	%mm1, %mm3
/	pfsub	%mm5, %mm7
	subps	%xmm1, %xmm2

/	pfmul	%mm2, %mm3
/	pfmul	%mm6, %mm7
	mulps	%xmm4, %xmm2

/	pswapd	%mm3, %mm3
/	pswapd	%mm7, %mm7
	shufps	$27, %xmm2, %xmm2

/	movq	%mm3, 88(%edx)
/	movq	%mm7, 80(%edx)
	movups	%xmm2, 80(%edx)


/	movq	96(%ecx), %mm0
/	movq	104(%ecx), %mm4
	movups	96(%ecx), %xmm0

/	movq	%mm0, %mm3
/	movq	%mm4, %mm7
	movaps	%xmm0, %xmm2

/	movq	120(%ecx), %mm1
/	movq	112(%ecx), %mm5
	movups	112(%ecx), %xmm1

/	pswapd	%mm1, %mm1
/	pswapd	%mm5, %mm5
////	shufps	$177, %xmm1, %xmm1
	shufps	$27, %xmm1, %xmm1

/	pfadd	%mm1, %mm0
/	pfadd	%mm5, %mm4
	addps	%xmm1, %xmm0

/	movq	%mm0, 96(%edx)
/	movq	%mm4, 104(%edx)
	movups	%xmm0, 96(%edx)

/	pfsubr	%mm1, %mm3
/	pfsubr	%mm5, %mm7
// This is already optimized, so xmm2 must be swapped with xmm1 for rest of phase
	subps	%xmm2, %xmm1

/	pfmul	%mm2, %mm3
/	pfmul	%mm6, %mm7
	mulps	%xmm4, %xmm1

/	pswapd	%mm3, %mm3
/	pswapd	%mm7, %mm7
	shufps	$27, %xmm1, %xmm1

/	movq	%mm3, 120(%edx)
/	movq	%mm7, 112(%edx)
	movups	%xmm1, 112(%edx)
*/

// phase 4 fpu code
/* Phase 4*/

	flds     (%ecx)
	fadds  28(%ecx)
	fstps    (%edx)

	flds     (%ecx)
	fsubs  28(%ecx)
	fmuls  96(%ebx)
	fstps  28(%edx)

	flds    4(%ecx)
	fadds  24(%ecx)
	fstps   4(%edx)

	flds    4(%ecx)
	fsubs  24(%ecx)
	fmuls 100(%ebx)
	fstps  24(%edx)

	flds    8(%ecx)
	fadds  20(%ecx)
	fstps   8(%edx)

	flds    8(%ecx)
	fsubs  20(%ecx)
	fmuls 104(%ebx)
	fstps  20(%edx)

	flds   12(%ecx)
	fadds  16(%ecx)
	fstps  12(%edx)

	flds   12(%ecx)
	fsubs  16(%ecx)
	fmuls 108(%ebx)
	fstps  16(%edx)

	flds   32(%ecx)
	fadds  60(%ecx)
	fstps  32(%edx)

	flds   60(%ecx)
	fsubs  32(%ecx)
	fmuls  96(%ebx)
	fstps  60(%edx)

	flds   36(%ecx)
	fadds  56(%ecx)
	fstps  36(%edx)

	flds   56(%ecx)
	fsubs  36(%ecx)
	fmuls 100(%ebx)
	fstps  56(%edx)

	flds   40(%ecx)
	fadds  52(%ecx)
	fstps  40(%edx)

	flds   52(%ecx)
	fsubs  40(%ecx)
	fmuls 104(%ebx)
	fstps  52(%edx)

	flds   44(%ecx)
	fadds  48(%ecx)
	fstps  44(%edx)

	flds   48(%ecx)
	fsubs  44(%ecx)
	fmuls 108(%ebx)
	fstps  48(%edx)

	flds   64(%ecx)
	fadds  92(%ecx)
	fstps  64(%edx)

	flds   64(%ecx)
	fsubs  92(%ecx)
	fmuls  96(%ebx)
	fstps  92(%edx)

	flds   68(%ecx)
	fadds  88(%ecx)
	fstps  68(%edx)

	flds   68(%ecx)
	fsubs  88(%ecx)
	fmuls 100(%ebx)
	fstps  88(%edx)

	flds   72(%ecx)
	fadds  84(%ecx)
	fstps  72(%edx)

	flds   72(%ecx)
	fsubs  84(%ecx)
	fmuls 104(%ebx)
	fstps  84(%edx)

	flds   76(%ecx)
	fadds  80(%ecx)
	fstps  76(%edx)

	flds   76(%ecx)
	fsubs  80(%ecx)
	fmuls 108(%ebx)
	fstps  80(%edx)

	flds   96(%ecx)
	fadds 124(%ecx)
	fstps  96(%edx)

	flds  124(%ecx)
	fsubs  96(%ecx)
	fmuls  96(%ebx)
	fstps 124(%edx)

	flds  100(%ecx)
	fadds 120(%ecx)
	fstps 100(%edx)

	flds  120(%ecx)
	fsubs 100(%ecx)
	fmuls 100(%ebx)
	fstps 120(%edx)

	flds  104(%ecx)
	fadds 116(%ecx)
	fstps 104(%edx)

	flds  116(%ecx)
	fsubs 104(%ecx)
	fmuls 104(%ebx)
	fstps 116(%edx)

	flds  108(%ecx)
	fadds 112(%ecx)
	fstps 108(%edx)

	flds  112(%ecx)
	fsubs 108(%ecx)
	fmuls 108(%ebx)
	fstps 112(%edx)

	flds     (%edx)
	fadds  12(%edx)
	fstps    (%ecx)

	flds     (%edx)
	fsubs  12(%edx)
	fmuls 112(%ebx)
	fstps  12(%ecx)

	flds    4(%edx)
	fadds   8(%edx)
	fstps   4(%ecx)

	flds    4(%edx)
	fsubs   8(%edx)
	fmuls 116(%ebx)
	fstps   8(%ecx)

	flds   16(%edx)
	fadds  28(%edx)
	fstps  16(%ecx)

	flds   28(%edx)
	fsubs  16(%edx)
	fmuls 112(%ebx)
	fstps  28(%ecx)

	flds   20(%edx)
	fadds  24(%edx)
	fstps  20(%ecx)

	flds   24(%edx)
	fsubs  20(%edx)
	fmuls 116(%ebx)
	fstps  24(%ecx)

	flds   32(%edx)
	fadds  44(%edx)
	fstps  32(%ecx)

	flds   32(%edx)
	fsubs  44(%edx)
	fmuls 112(%ebx)
	fstps  44(%ecx)

	flds   36(%edx)
	fadds  40(%edx)
	fstps  36(%ecx)

	flds   36(%edx)
	fsubs  40(%edx)
	fmuls 116(%ebx)
	fstps  40(%ecx)

	flds   48(%edx)
	fadds  60(%edx)
	fstps  48(%ecx)

	flds   60(%edx)
	fsubs  48(%edx)
	fmuls 112(%ebx)
	fstps  60(%ecx)

	flds   52(%edx)
	fadds  56(%edx)
	fstps  52(%ecx)

	flds   56(%edx)
	fsubs  52(%edx)
	fmuls 116(%ebx)
	fstps  56(%ecx)

	flds   64(%edx)
	fadds  76(%edx)
	fstps  64(%ecx)

	flds   64(%edx)
	fsubs  76(%edx)
	fmuls 112(%ebx)
	fstps  76(%ecx)

	flds   68(%edx)
	fadds  72(%edx)
	fstps  68(%ecx)

	flds   68(%edx)
	fsubs  72(%edx)
	fmuls 116(%ebx)
	fstps  72(%ecx)

	flds   80(%edx)
	fadds  92(%edx)
	fstps  80(%ecx)

	flds   92(%edx)
	fsubs  80(%edx)
	fmuls 112(%ebx)
	fstps  92(%ecx)

	flds   84(%edx)
	fadds  88(%edx)
	fstps  84(%ecx)

	flds   88(%edx)
	fsubs  84(%edx)
	fmuls 116(%ebx)
	fstps  88(%ecx)

	flds   96(%edx)
	fadds 108(%edx)
	fstps  96(%ecx)

	flds   96(%edx)
	fsubs 108(%edx)
	fmuls 112(%ebx)
	fstps 108(%ecx)

	flds  100(%edx)
	fadds 104(%edx)
	fstps 100(%ecx)

	flds  100(%edx)
	fsubs 104(%edx)
	fmuls 116(%ebx)
	fstps 104(%ecx)

	flds  112(%edx)
	fadds 124(%edx)
	fstps 112(%ecx)

	flds  124(%edx)
	fsubs 112(%edx)
	fmuls 112(%ebx)
	fstps 124(%ecx)

	flds  116(%edx)
	fadds 120(%edx)
	fstps 116(%ecx)

	flds  120(%edx)
	fsubs 116(%edx)
	fmuls 116(%ebx)
	fstps 120(%ecx)
	
// end of phase 4 fpu

// below stuff needs to be finished I use FPU code for first
/* Phase 5 (completed, crashing) */
/*
/	movq	112(%ebx), %mm2
	// move 8 byte data to (low)high quadword - check this! atmos
	movlps	112(%ebx), %xmm4
	// maybe I need movhlps too to get data into correct quadword
	movlhps	%xmm4, %xmm4

/	movq	(%edx), %mm0
/	movq	16(%edx), %mm4
	movups	(%edx), %xmm0

/	movq	%mm0, %mm3
/	movq	%mm4, %mm7
	movaps	%xmm0, %xmm2

// hmm? this is strange
/	movq	8(%edx), %mm1
/	movq	24(%edx), %mm5
	movlps	8(%edx), %xmm1
	movhps	24(%edx), %xmm1

/	pswapd	%mm1, %mm1
/	pswapd	%mm5, %mm5
	pshufd	$177, %xmm1, %xmm1

/	pfadd	%mm1, %mm0
/	pfadd	%mm5, %mm4
	addps	%xmm1, %xmm0

/	movq	%mm0, (%ecx)
/	movq	%mm4, 16(%ecx)
	movlps	%xmm0, (%ecx)
	movhps	%xmm0, 16(%ecx)

/	pfsub	%mm1, %mm3
/	pfsubr	%mm5, %mm7
// I need to emulate pfsubr here
	movaps	%xmm1, %xmm3
	subps	%xmm2, %xmm3
	subps	%xmm1, %xmm2
// now move correct quadword from reverse substration in xmm3 to correct
// quadword in xmm2 and leave other quadword with non-reversed substration untouched 
///	shufpd	$2, %xmm3, %xmm2
// (or $1?) (see ia32-ref p.749)
// optimize
	movq	%xmm2, %xmm3
	movaps	%xmm3, %xmm2

/	pfmul	%mm2, %mm3
/	pfmul	%mm2, %mm7
	mulps	%xmm4, %xmm2

/	pswapd	%mm3, %mm3
/	pswapd	%mm7, %mm7
	shufps	$177, %xmm2, %xmm2

/	movq	%mm3, 8(%ecx)
/	movq	%mm7, 24(%ecx)
	movlps	%xmm2, 8(%ecx)
	movhps	%xmm2, 24(%ecx)

/	movq	32(%edx), %mm0
/	movq	48(%edx), %mm4
	movlps	32(%edx), %xmm0
	movhps	48(%edx), %xmm0

/	movq	%mm0, %mm3
/	movq	%mm4, %mm7
	movaps	%xmm0, %xmm2

/	movq	40(%edx), %mm1
/	movq	56(%edx), %mm5
	movlps	40(%edx), %xmm1
	movhps	56(%edx), %xmm1

/	pswapd	%mm1, %mm1
/	pswapd	%mm5, %mm5
	shufps	$177, %xmm1, %xmm1

/	pfadd	%mm1, %mm0
/	pfadd	%mm5, %mm4
	addps	%xmm1, %xmm0

/	movq	%mm0, 32(%ecx)
/	movq	%mm4, 48(%ecx)
	movlps	%xmm0, 32(%ecx)
	movhps	%xmm0, 48(%ecx)

/	pfsub	%mm1, %mm3
/	pfsubr	%mm5, %mm7
	movaps	%xmm1, %xmm3
	subps	%xmm2, %xmm3
	subps	%xmm1, %xmm2
///	shufpd	$2, %xmm3, %xmm2
// (or $1?)
// optimize
	movq	%xmm2, %xmm3
	movaps	%xmm3, %xmm2

/	pfmul	%mm2, %mm3
/	pfmul	%mm2, %mm7
	mulps	%xmm4, %xmm2

/	pswapd	%mm3, %mm3
/	pswapd	%mm7, %mm7
	shufps	$177, %xmm2, %xmm2

/	movq	%mm3, 40(%ecx)
/	movq	%mm7, 56(%ecx)
	movlps	%xmm2, 40(%ecx)
	movhps	%xmm2, 56(%ecx)


/	movq	64(%edx), %mm0
/	movq	80(%edx), %mm4
	movlps	64(%edx), %xmm0
	movhps	80(%edx), %xmm0

/	movq	%mm0, %mm3
/	movq	%mm4, %mm7
	movaps	%xmm0, %xmm2

/	movq	72(%edx), %mm1
/	movq	88(%edx), %mm5
	movlps	72(%edx), %xmm1
	movhps	88(%edx), %xmm1

/	pswapd	%mm1, %mm1
/	pswapd	%mm5, %mm5
	shufps	$177, %xmm1, %xmm1

/	pfadd	%mm1, %mm0
/	pfadd	%mm5, %mm4
	addps	%xmm1, %xmm0

/	movq	%mm0, 64(%ecx)
/	movq	%mm4, 80(%ecx)
	movlps	%xmm0, 64(%ecx)
	movhps	%xmm0, 80(%ecx)

/	pfsub	%mm1, %mm3
/	pfsubr	%mm5, %mm7
	movaps	%xmm1, %xmm3
	subps	%xmm2, %xmm3
	subps	%xmm1, %xmm2
///	shufpd	$2, %xmm3, %xmm2
// (or $1?)
// optimize
	movq	%xmm2, %xmm3
	movaps	%xmm3, %xmm2

/	pfmul	%mm2, %mm3
/	pfmul	%mm2, %mm7
	mulps	%xmm4, %xmm2

/	pswapd	%mm3, %mm3
/	pswapd	%mm7, %mm7
	shufps	$177, %xmm2, %xmm2

/	movq	%mm3, 72(%ecx)
/	movq	%mm7, 88(%ecx)
	movlps	%xmm2, 72(%ecx)
	movhps	%xmm2, 88(%ecx)

/	movq	96(%edx), %mm0
/	movq	112(%edx), %mm4
	movups	96(%edx), %xmm0

/	movq	%mm0, %mm3
/	movq	%mm4, %mm7
	movaps	%xmm0, %xmm2

/	movq	104(%edx), %mm1
/	movq	120(%edx), %mm5
	movlps	104(%edx), %xmm1
	movhps	120(%edx), %xmm1

/	pswapd	%mm1, %mm1
/	pswapd	%mm5, %mm5
	shufps	$177, %xmm1, %xmm1

/	pfadd	%mm1, %mm0
/	pfadd	%mm5, %mm4
	addps	%xmm1, %xmm0

/	movq	%mm0, 96(%ecx)
/	movq	%mm4, 112(%ecx)
	movups	%xmm0, 96(%ecx)

/	pfsub	%mm1, %mm3
/	pfsubr	%mm5, %mm7
	movaps	%xmm1, %xmm3
	subps	%xmm2, %xmm3
	subps	%xmm1, %xmm2
///	shufpd	$2, %xmm3, %xmm2
// (or $1?)
// optimize
	movq	%xmm2, %xmm3
	movaps	%xmm3, %xmm2

/	pfmul	%mm2, %mm3
/	pfmul	%mm2, %mm7
	mulps	%xmm4, %xmm2

/	pswapd	%mm3, %mm3
/	pswapd	%mm7, %mm7
	shufps	$177, %xmm2, %xmm2

/	movq	%mm3, 104(%ecx)
/	movq	%mm7, 120(%ecx)
	movlps	%xmm2, 104(%ecx)
	movhps	%xmm2, 120(%ecx)
*/
	
	
/* Phase 6. This is the end of easy road. */
/* Code below is coded in scalar mode. Should be optimized */
//
//	movd	plus_1f, %mm6
//	punpckldq 120(%ebx), %mm6      /* mm6 = 1.0 | 120(%ebx)*/
//	movq	x_plus_minus_3dnow, %mm7 /* mm7 = +1 | -1 */
/*
	movq	32(%ecx), %mm0
	movq	64(%ecx), %mm2
	movq	%mm0, %mm1
	movq	%mm2, %mm3
	pxor	%mm7, %mm1
	pxor	%mm7, %mm3
	pfacc	%mm1, %mm0
	pfacc	%mm3, %mm2
	pfmul	%mm6, %mm0
	pfmul	%mm6, %mm2
	movq	%mm0, 32(%edx)
	movq	%mm2, 64(%edx)

	movd	44(%ecx), %mm0
	movd	40(%ecx), %mm2
	movd	120(%ebx), %mm3
	punpckldq 76(%ecx), %mm0
	punpckldq 72(%ecx), %mm2
	punpckldq %mm3, %mm3
	movq	%mm0, %mm4
	movq	%mm2, %mm5
	pfsub	%mm2, %mm0
	pfmul	%mm3, %mm0
	movq	%mm0, %mm1
	pfadd	%mm5, %mm0
	pfadd	%mm4, %mm0
	movq	%mm0, %mm2
	punpckldq %mm1, %mm0
	punpckhdq %mm1, %mm2
	movq	%mm0, 40(%edx)
	movq	%mm2, 72(%edx)

	movd   48(%ecx), %mm3
	movd   60(%ecx), %mm2
	pfsub  52(%ecx), %mm3
	pfsub  56(%ecx), %mm2
	pfmul 120(%ebx), %mm3
	pfmul 120(%ebx), %mm2
	movq	%mm2, %mm1

	pfadd  56(%ecx), %mm1
	pfadd  60(%ecx), %mm1
	movq	%mm1, %mm0

	pfadd  48(%ecx), %mm0
	pfadd  52(%ecx), %mm0
	pfadd	%mm3, %mm1
	punpckldq %mm2, %mm1
	pfadd	%mm3, %mm2
	punpckldq %mm2, %mm0
	movq	%mm1, 56(%edx)
	movq	%mm0, 48(%edx)
*/
/*---*/
/*
	movd   92(%ecx), %mm1
	pfsub  88(%ecx), %mm1
	pfmul 120(%ebx), %mm1
	movd   %mm1, 92(%edx)
	pfadd  92(%ecx), %mm1
	pfadd  88(%ecx), %mm1
	movq   %mm1, %mm0
	
	pfadd  80(%ecx), %mm0
	pfadd  84(%ecx), %mm0
	movd   %mm0, 80(%edx)

	movd   80(%ecx), %mm0
	pfsub  84(%ecx), %mm0
	pfmul 120(%ebx), %mm0
	pfadd  %mm0, %mm1
	pfadd  92(%edx), %mm0
	punpckldq %mm1, %mm0
	movq   %mm0, 84(%edx)

	movq	96(%ecx), %mm0
	movq	%mm0, %mm1
	pxor	%mm7, %mm1
	pfacc	%mm1, %mm0
	pfmul	%mm6, %mm0
	movq	%mm0, 96(%edx)

	movd  108(%ecx), %mm0
	pfsub 104(%ecx), %mm0
	pfmul 120(%ebx), %mm0
	movd  %mm0, 108(%edx)
	pfadd 104(%ecx), %mm0
	pfadd 108(%ecx), %mm0
	movd  %mm0, 104(%edx)

	movd  124(%ecx), %mm1
	pfsub 120(%ecx), %mm1
	pfmul 120(%ebx), %mm1
	movd  %mm1, 124(%edx)
	pfadd 120(%ecx), %mm1
	pfadd 124(%ecx), %mm1
	movq  %mm1, %mm0

	pfadd 112(%ecx), %mm0
	pfadd 116(%ecx), %mm0
	movd  %mm0, 112(%edx)

	movd  112(%ecx), %mm0
	pfsub 116(%ecx), %mm0
	pfmul 120(%ebx), %mm0
	pfadd %mm0,%mm1
	pfadd 124(%edx), %mm0
	punpckldq %mm1, %mm0
	movq  %mm0, 116(%edx)

	jnz .L01
*/

	
/* Phase 7*/
/* Code below is coded in scalar mode. Should be optimized */
/*
	movd      (%ecx), %mm0
	pfadd    4(%ecx), %mm0
	movd     %mm0, 1024(%esi)

	movd      (%ecx), %mm0
	pfsub    4(%ecx), %mm0
	pfmul  120(%ebx), %mm0
	movd      %mm0, (%esi)
	movd      %mm0, (%edi)

	movd   12(%ecx), %mm0
	pfsub   8(%ecx), %mm0
	pfmul 120(%ebx), %mm0
	movd    %mm0, 512(%edi)
	pfadd   12(%ecx), %mm0
	pfadd   8(%ecx), %mm0
	movd    %mm0, 512(%esi)

	movd   16(%ecx), %mm0
	pfsub  20(%ecx), %mm0
	pfmul 120(%ebx), %mm0
	movq	%mm0, %mm3

	movd   28(%ecx), %mm0
	pfsub  24(%ecx), %mm0
	pfmul 120(%ebx), %mm0
	movd    %mm0, 768(%edi)
	movq	%mm0, %mm2
	
	pfadd  24(%ecx), %mm0
	pfadd  28(%ecx), %mm0
	movq	%mm0, %mm1

	pfadd  16(%ecx), %mm0
	pfadd  20(%ecx), %mm0
	movd   %mm0, 768(%esi)
	pfadd  %mm3, %mm1
	movd   %mm1, 256(%esi)
	pfadd  %mm3, %mm2
	movd   %mm2, 256(%edi)
*/

	
/* Phase 8*/
/*
	movq   32(%edx), %mm0
	movq   48(%edx), %mm1
	pfadd  48(%edx), %mm0
	pfadd  40(%edx), %mm1
	movd   %mm0, 896(%esi)
	movd   %mm1, 640(%esi)
	psrlq  $32, %mm0
	psrlq  $32, %mm1
	movd   %mm0, 128(%edi)
	movd   %mm1, 384(%edi)

	movd   40(%edx), %mm0
	pfadd  56(%edx), %mm0
	movd   %mm0, 384(%esi)

	movd   56(%edx), %mm0
	pfadd  36(%edx), %mm0
	movd   %mm0, 128(%esi)

	movd   60(%edx), %mm0
	movd   %mm0, 896(%edi)
	pfadd  44(%edx), %mm0
	movd   %mm0, 640(%edi)

	movq   96(%edx), %mm0
	movq   112(%edx), %mm2
	movq   104(%edx), %mm4
	pfadd  112(%edx), %mm0
	pfadd  104(%edx), %mm2
	pfadd  120(%edx), %mm4
	movq   %mm0, %mm1
	movq   %mm2, %mm3
	movq   %mm4, %mm5
	pfadd  64(%edx), %mm0
	pfadd  80(%edx), %mm2
	pfadd  72(%edx), %mm4
	movd   %mm0, 960(%esi)
	movd   %mm2, 704(%esi)
	movd   %mm4, 448(%esi)
	psrlq  $32, %mm0
	psrlq  $32, %mm2
	psrlq  $32, %mm4
	movd   %mm0, 64(%edi)
	movd   %mm2, 320(%edi)
	movd   %mm4, 576(%edi)
	pfadd  80(%edx), %mm1
	pfadd  72(%edx), %mm3
	pfadd  88(%edx), %mm5
	movd   %mm1, 832(%esi)
	movd   %mm3, 576(%esi)
	movd   %mm5, 320(%esi)
	psrlq  $32, %mm1
	psrlq  $32, %mm3
	psrlq  $32, %mm5
	movd   %mm1, 192(%edi)
	movd   %mm3, 448(%edi)
	movd   %mm5, 704(%edi)

	movd   120(%edx), %mm0
	pfadd  100(%edx), %mm0
	movq   %mm0, %mm1
	pfadd  88(%edx), %mm0
	movd   %mm0, 192(%esi)
	pfadd  68(%edx), %mm1
	movd   %mm1, 64(%esi)

	movd  124(%edx), %mm0
	movd  %mm0, 960(%edi)
	pfadd  92(%edx), %mm0
	movd  %mm0, 832(%edi)

	jmp	.L_bye
.L01:
*/

	
/* Phase 9*/
/*
	movq	(%ecx), %mm0
	movq	%mm0, %mm1
	pxor    %mm7, %mm1
	pfacc	%mm1, %mm0
	pfmul	%mm6, %mm0
	pf2id	%mm0, %mm0
	movd	%mm0, %eax
	movw    %ax, 512(%esi)
	psrlq	$32, %mm0
	movd	%mm0, %eax
	movw    %ax, (%esi)

	movd    12(%ecx), %mm0
	pfsub    8(%ecx), %mm0
	pfmul  120(%ebx), %mm0
	pf2id    %mm0, %mm7
	movd	 %mm7, %eax
	movw     %ax, 256(%edi)
	pfadd   12(%ecx), %mm0
	pfadd    8(%ecx), %mm0
	pf2id    %mm0, %mm0
	movd	 %mm0, %eax
	movw     %ax, 256(%esi)

	movd   16(%ecx), %mm3
	pfsub  20(%ecx), %mm3
	pfmul  120(%ebx), %mm3
	movq   %mm3, %mm2

	movd   28(%ecx), %mm2
	pfsub  24(%ecx), %mm2
	pfmul 120(%ebx), %mm2
	movq   %mm2, %mm1

	pf2id  %mm2, %mm7
	movd   %mm7, %eax
	movw   %ax, 384(%edi)
	
	pfadd  24(%ecx), %mm1
	pfadd  28(%ecx), %mm1
	movq   %mm1, %mm0
	
	pfadd  16(%ecx), %mm0
	pfadd  20(%ecx), %mm0
	pf2id  %mm0, %mm0
	movd   %mm0, %eax
	movw   %ax, 384(%esi)
	pfadd  %mm3, %mm1
	pf2id  %mm1, %mm1
	movd   %mm1, %eax
	movw   %ax, 128(%esi)
	pfadd  %mm3, %mm2
	pf2id  %mm2, %mm2
	movd   %mm2, %eax
	movw   %ax, 128(%edi)
*/

	
/* Phase 10*/
/*
	movq    32(%edx), %mm0
	movq    48(%edx), %mm1
	pfadd   48(%edx), %mm0
	pfadd   40(%edx), %mm1
	pf2id   %mm0, %mm0
	pf2id   %mm1, %mm1
	movd	%mm0, %eax
	movd	%mm1, %ecx
	movw    %ax, 448(%esi)
	movw    %cx, 320(%esi)
	psrlq   $32, %mm0
	psrlq   $32, %mm1
	movd	%mm0, %eax
	movd	%mm1, %ecx
	movw    %ax, 64(%edi)
	movw    %cx, 192(%edi)

	movd   40(%edx), %mm3
	movd   56(%edx), %mm4
	movd   60(%edx), %mm0
	movd   44(%edx), %mm2
	movd  120(%edx), %mm5
	punpckldq %mm4, %mm3
	punpckldq 124(%edx), %mm0
	pfadd 100(%edx), %mm5
	punpckldq 36(%edx), %mm4
	punpckldq 92(%edx), %mm2	
	movq  %mm5, %mm6
	pfadd  %mm4, %mm3
	pf2id  %mm0, %mm1
	pf2id  %mm3, %mm3
	pfadd  88(%edx), %mm5
	movd   %mm1, %eax
	movd   %mm3, %ecx
	movw   %ax, 448(%edi)
	movw   %cx, 192(%esi)
	pf2id  %mm5, %mm5
	psrlq  $32, %mm1
        psrlq  $32, %mm3
	movd   %mm5, %ebx
	movd   %mm1, %eax
	movd   %mm3, %ecx
	movw   %bx, 96(%esi)
	movw   %ax, 480(%edi)
	movw   %cx, 64(%esi)
	pfadd  %mm2, %mm0
	pf2id  %mm0, %mm0
	movd   %mm0, %eax
	pfadd  68(%edx), %mm6
	movw   %ax, 320(%edi)
	psrlq  $32, %mm0
	pf2id  %mm6, %mm6
	movd   %mm0, %eax
	movd   %mm6, %ebx
	movw   %ax, 416(%edi)
	movw   %bx, 32(%esi)

	movq   96(%edx), %mm0
	movq  112(%edx), %mm2
	movq  104(%edx), %mm4
	pfadd %mm2, %mm0
	pfadd %mm4, %mm2
	pfadd 120(%edx), %mm4
	movq  %mm0, %mm1
	movq  %mm2, %mm3
	movq  %mm4, %mm5
	pfadd  64(%edx), %mm0
	pfadd  80(%edx), %mm2
	pfadd  72(%edx), %mm4
	pf2id  %mm0, %mm0
	pf2id  %mm2, %mm2
	pf2id  %mm4, %mm4
	movd   %mm0, %eax
	movd   %mm2, %ecx
	movd   %mm4, %ebx
	movw   %ax, 480(%esi)
	movw   %cx, 352(%esi)
	movw   %bx, 224(%esi)
	psrlq  $32, %mm0
	psrlq  $32, %mm2
	psrlq  $32, %mm4
	movd   %mm0, %eax
	movd   %mm2, %ecx
	movd   %mm4, %ebx
	movw   %ax, 32(%edi)
	movw   %cx, 160(%edi)
	movw   %bx, 288(%edi)
	pfadd  80(%edx), %mm1
	pfadd  72(%edx), %mm3
	pfadd  88(%edx), %mm5
	pf2id  %mm1, %mm1
	pf2id  %mm3, %mm3
	pf2id  %mm5, %mm5
	movd   %mm1, %eax
	movd   %mm3, %ecx
	movd   %mm5, %ebx
	movw   %ax, 416(%esi)
	movw   %cx, 288(%esi)
	movw   %bx, 160(%esi)
	psrlq  $32, %mm1
	psrlq  $32, %mm3
	psrlq  $32, %mm5
	movd   %mm1, %eax
	movd   %mm3, %ecx
	movd   %mm5, %ebx
	movw   %ax, 96(%edi)
	movw   %cx, 224(%edi)
	movw   %bx, 352(%edi)

	movsw

.L_bye:
	addl $256,%esp
/	femms
	emms
	popl %edi
	popl %esi
	popl %ebx
	ret  $12
*/

// here comes old fashioned FPU code for the tough parts

/* Phase 5*/

	flds   32(%ecx)
	fadds  36(%ecx)
	fstps  32(%edx)

	flds   32(%ecx)
	fsubs  36(%ecx)
	fmuls 120(%ebx)
	fstps  36(%edx)

	flds   44(%ecx)
	fsubs  40(%ecx)
	fmuls 120(%ebx)
	fsts   44(%edx)
	fadds  40(%ecx)
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

	
/* Phase 6*/

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
	
/* Phase 7*/

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
	addl $256,%esp
	popl %edi
	popl %esi
	popl %ebx
	ret
.L01:	
/* Phase 8*/

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
	
/* Phase 9*/

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
	addl $256,%esp
	popl %edi
	popl %esi
	popl %ebx
	ret	$12

// end of FPU stuff
