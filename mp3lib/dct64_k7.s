///
/// Replacement of dct64() with AMD's 3DNowEx(DSP)! SIMD operations support
///
/// This code based 'dct64_3dnow.s' by Syuuhei Kashiyama
/// <squash@mb.kcom.ne.jp>,only some types of changes have been made:
///
///  - added new opcodes PSWAPD, PFPNACC
///  - decreased number of opcodes (as it was suggested by k7 manual)
///    (using memory reference as operand of instructions)
///  - Phase 6 is rewritten with mixing of cpu and mmx opcodes
///  - change function name for support 3DNowEx! automatic detect
///  - negation of 3dnow reg was replaced with PXOR 0x800000000, MMi instead 
///    of PFMUL as it was suggested by athlon manual. (Two not separated PFMUL
///    can not be paired, but PXOR can be).
///
/// note: because K7 processors are an aggresive out-of-order three-way
///       superscalar ones instruction order is not significand for them.
///
/// Modified by Nick Kurshev <nickols_k@mail.ru>
///
/// The author of this program disclaim whole expressed or implied
/// warranties with regard to this program, and in no event shall the
/// author of this program liable to whatever resulted from the use of
/// this program. Use it at your own risk.
///

.data
        .align 8
plus_minus_3dnow: .long 0x00000000, 0x80000000

.text
        .globl dct64_3dnowex
        .type    dct64_3dnowex,@function

/* Discrete Cosine Tansform (DCT) for subband synthesis */
/* void dct64(real *a,real *b,real *c) */
dct64_3dnowex:
        subl $256,%esp
        pushl %ebp
        pushl %edi
        pushl %esi
        pushl %ebx
        leal 16(%esp),%ebx   /* ebx -> real tmp1[32] */
        movl 284(%esp),%edi  /* edi -> c */
        movl 276(%esp),%ebp  /* ebp -> a */
        movl 280(%esp),%edx  /* edx -> b */
        leal 128(%ebx),%esi  /* esi -> real tmp2[32] */

        / femms

        // 1
        movl pnts,%eax

        movq 0(%edi),%mm0        /* mm0 = c[0x00] | c[0x01]*/
        movq %mm0,%mm1           /* mm1 = mm0 */
        movd 124(%edi),%mm2      /* mm2 = c[0x1f] */
        punpckldq 120(%edi),%mm2 /* mm2 = c[0x1f] | c[0x1E] */
        pfadd %mm2,%mm0          /* mm0 = c[0x00]+c[0x1F] | c[0x1E]+c[0x01] */
        movq %mm0,0(%ebx)        /* tmp[0, 1] = mm0 */
        pfsub %mm2,%mm1          /* c[0x00]-c[0x1f] | c[0x01]-c[0x1e] */
        pfmul 0(%eax),%mm1       /* (c[0x00]-c[0x1f])*pnts[0]|(c[0x01]-c[0x1e])*pnts[1]*/
        pswapd %mm1, %mm1        /* (c[0x01]-c[0x1e])*pnts[1]|(c[0x00]-c[0x1f])*pnts[0]*/
        movq   %mm1, 120(%ebx)   /* tmp1[30, 31]=mm1 */

        movq 8(%edi),%mm4
        movq %mm4,%mm5
        movd 116(%edi),%mm6
        punpckldq 112(%edi),%mm6
        pfadd %mm6,%mm4
        movq %mm4,8(%ebx)
        pfsub %mm6,%mm5
        pfmul 8(%eax),%mm5
        pswapd %mm5, %mm5
        movq   %mm5, 112(%ebx)

        movq 16(%edi),%mm0
        movq %mm0,%mm1
        movd 108(%edi),%mm2
        punpckldq 104(%edi),%mm2
        pfadd %mm2,%mm0
        movq %mm0,16(%ebx)
        pfsub %mm2,%mm1
        pfmul 16(%eax),%mm1
        pswapd %mm1, %mm1
        movq   %mm1, 104(%ebx)

        movq 24(%edi),%mm4
        movq %mm4,%mm5
        movd 100(%edi),%mm6
        punpckldq 96(%edi),%mm6
        pfadd %mm6,%mm4
        movq %mm4,24(%ebx)
        pfsub %mm6,%mm5
        pfmul 24(%eax),%mm5
        pswapd %mm5, %mm5
        movq   %mm5, 96(%ebx)

        movq 32(%edi),%mm0
        movq %mm0,%mm1
        movd 92(%edi),%mm2
        punpckldq 88(%edi),%mm2
        pfadd %mm2,%mm0
        movq %mm0,32(%ebx)
        pfsub %mm2,%mm1
        pfmul 32(%eax),%mm1
        pswapd %mm1, %mm1
        movq   %mm1, 88(%ebx)

        movq 40(%edi),%mm4
        movq %mm4,%mm5
        movd 84(%edi),%mm6
        punpckldq 80(%edi),%mm6
        pfadd %mm6,%mm4
        movq %mm4,40(%ebx)
        pfsub %mm6,%mm5
        pfmul 40(%eax),%mm5
        pswapd %mm5, %mm5
        movq   %mm5, 80(%ebx)

        movq 48(%edi),%mm0
        movq %mm0,%mm1
        movd 76(%edi),%mm2
        punpckldq 72(%edi),%mm2
        pfadd %mm2,%mm0
        movq %mm0,48(%ebx)
        pfsub %mm2,%mm1
        pfmul 48(%eax),%mm1
        pswapd %mm1, %mm1
        movq   %mm1, 72(%ebx)

        movq 56(%edi),%mm4
        movq %mm4,%mm5
        movd 68(%edi),%mm6
        punpckldq 64(%edi),%mm6
        pfadd %mm6,%mm4
        movq %mm4,56(%ebx)
        pfsub %mm6,%mm5
        pfmul 56(%eax),%mm5
        pswapd %mm5, %mm5
        movq   %mm5, 64(%ebx)

        // 2
        movl pnts+4,%eax
        / 0, 14
        movq 0(%ebx),%mm0            /* mm0 = tmp1[0] | tmp1[1] */
        movq %mm0,%mm1
        movd 60(%ebx),%mm2           /* mm2 = tmp1[0x0F] */
        punpckldq 56(%ebx),%mm2      /* mm2 = tmp1[0x0E] | tmp1[0x0F] */
        movq 0(%eax),%mm3            /* mm3 = pnts[0] | pnts[1] */
        pfadd %mm2,%mm0              /* mm0 = tmp1[0]+tmp1[0x0F]|tmp1[1]+tmp1[0x0E]*/
        movq %mm0,0(%esi)            /* tmp2[0, 1] = mm0 */
        pfsub %mm2,%mm1              /* mm1 = tmp1[0]-tmp1[0x0F]|tmp1[1]-tmp1[0x0E]*/
        pfmul %mm3,%mm1              /* mm1 = (tmp1[0]-tmp1[0x0F])*pnts[0]|(tmp1[1]-tmp1[0x0E])*pnts[1]*/
        pswapd %mm1, %mm1            /* mm1 = (tmp1[1]-tmp1[0x0E])*pnts[1]|(tmp1[0]-tmp1[0x0F])*pnts[0]*/
        movq   %mm1, 56(%esi)        /* tmp2[0x0E, 0x0F] = mm1 */
        / 16, 30
        movq 64(%ebx),%mm0
        movq %mm0,%mm1
        movd 124(%ebx),%mm2
        punpckldq 120(%ebx),%mm2
        pfadd %mm2,%mm0
        movq %mm0,64(%esi)
        pfsubr %mm2,%mm1
        pfmul %mm3,%mm1
        pswapd %mm1, %mm1
        movq   %mm1, 120(%esi)
        movq 8(%ebx),%mm4
        / 2, 12
        movq %mm4,%mm5
        movd 52(%ebx),%mm6
        punpckldq 48(%ebx),%mm6
        movq 8(%eax),%mm7
        pfadd %mm6,%mm4
        movq %mm4,8(%esi)
        pfsub %mm6,%mm5
        pfmul %mm7,%mm5
        pswapd %mm5, %mm5
        movq   %mm5, 48(%esi)
        movq 72(%ebx),%mm4
        / 18, 28
        movq %mm4,%mm5
        movd 116(%ebx),%mm6
        punpckldq 112(%ebx),%mm6
        pfadd %mm6,%mm4
        movq %mm4,72(%esi)
        pfsubr %mm6,%mm5
        pfmul %mm7,%mm5
        pswapd %mm5, %mm5
        movq   %mm5, 112(%esi)
        movq 16(%ebx),%mm0
        / 4, 10
        movq %mm0,%mm1
        movd 44(%ebx),%mm2
        punpckldq 40(%ebx),%mm2
        movq 16(%eax),%mm3
        pfadd %mm2,%mm0
        movq %mm0,16(%esi)
        pfsub %mm2,%mm1
        pfmul %mm3,%mm1
        pswapd %mm1, %mm1
        movq   %mm1, 40(%esi)
        movq 80(%ebx),%mm0
        / 20, 26
        movq %mm0,%mm1
        movd 108(%ebx),%mm2
        punpckldq 104(%ebx),%mm2
        pfadd %mm2,%mm0
        movq %mm0,80(%esi)
        pfsubr %mm2,%mm1
        pfmul %mm3,%mm1
        pswapd %mm1, %mm1
        movq   %mm1, 104(%esi)
        movq 24(%ebx),%mm4
        / 6, 8
        movq %mm4,%mm5
        movd 36(%ebx),%mm6
        punpckldq 32(%ebx),%mm6
        movq 24(%eax),%mm7
        pfadd %mm6,%mm4
        movq %mm4,24(%esi)
        pfsub %mm6,%mm5
        pfmul %mm7,%mm5
        pswapd %mm5, %mm5
        movq   %mm5, 32(%esi)
        movq 88(%ebx),%mm4
        / 22, 24
        movq %mm4,%mm5
        movd 100(%ebx),%mm6
        punpckldq 96(%ebx),%mm6
        pfadd %mm6,%mm4
        movq %mm4,88(%esi)
        pfsubr %mm6,%mm5
        pfmul %mm7,%mm5
        pswapd %mm5, %mm5
        movq   %mm5, 96(%esi)

        // 3
        movl pnts+8,%eax
        movq 0(%eax),%mm0
        movq 8(%eax),%mm1
        movq 0(%esi),%mm2
        / 0, 6
        movq %mm2,%mm3
        movd 28(%esi),%mm4
        punpckldq 24(%esi),%mm4
        pfadd %mm4,%mm2
        pfsub %mm4,%mm3
        pfmul %mm0,%mm3
        movq %mm2,0(%ebx)
        pswapd %mm3, %mm3
        movq   %mm3, 24(%ebx)
        movq 8(%esi),%mm5
        / 2, 4
        movq %mm5,%mm6
        movd 20(%esi),%mm7
        punpckldq 16(%esi),%mm7
        pfadd %mm7,%mm5
        pfsub %mm7,%mm6
        pfmul %mm1,%mm6
        movq %mm5,8(%ebx)
        pswapd %mm6, %mm6
        movq   %mm6, 16(%ebx)
        movq 32(%esi),%mm2
        / 8, 14
        movq %mm2,%mm3
        movd 60(%esi),%mm4
        punpckldq 56(%esi),%mm4
        pfadd %mm4,%mm2
        pfsubr %mm4,%mm3
        pfmul %mm0,%mm3
        movq %mm2,32(%ebx)
        pswapd %mm3, %mm3
        movq   %mm3, 56(%ebx)
        movq 40(%esi),%mm5
        / 10, 12
        movq %mm5,%mm6
        movd 52(%esi),%mm7
        punpckldq 48(%esi),%mm7
        pfadd %mm7,%mm5
        pfsubr %mm7,%mm6
        pfmul %mm1,%mm6
        movq %mm5,40(%ebx)
        pswapd %mm6, %mm6
        movq   %mm6, 48(%ebx)
        movq 64(%esi),%mm2
        / 16, 22
        movq %mm2,%mm3
        movd 92(%esi),%mm4
        punpckldq 88(%esi),%mm4
        pfadd %mm4,%mm2
        pfsub %mm4,%mm3
        pfmul %mm0,%mm3
        movq %mm2,64(%ebx)
        pswapd %mm3, %mm3
        movq   %mm3, 88(%ebx)
        movq 72(%esi),%mm5
        / 18, 20
        movq %mm5,%mm6
        movd 84(%esi),%mm7
        punpckldq 80(%esi),%mm7
        pfadd %mm7,%mm5
        pfsub %mm7,%mm6
        pfmul %mm1,%mm6
        movq %mm5,72(%ebx)
        pswapd %mm6, %mm6
        movq   %mm6, 80(%ebx)
        movq 96(%esi),%mm2
        / 24, 30
        movq %mm2,%mm3
        movd 124(%esi),%mm4
        punpckldq 120(%esi),%mm4
        pfadd %mm4,%mm2
        pfsubr %mm4,%mm3
        pfmul %mm0,%mm3
        movq %mm2,96(%ebx)
        pswapd %mm3, %mm3
        movq   %mm3, 120(%ebx)
        movq 104(%esi),%mm5
        / 26, 28
        movq %mm5,%mm6
        movd 116(%esi),%mm7
        punpckldq 112(%esi),%mm7
        pfadd %mm7,%mm5
        pfsubr %mm7,%mm6
        pfmul %mm1,%mm6
        movq %mm5,104(%ebx)
        pswapd %mm6, %mm6
        movq   %mm6, 112(%ebx)

        // 4
        movl pnts+12,%eax    
        movq 0(%eax),%mm0      /* mm0 = pnts[3] | pnts[4] */
        movq 0(%ebx),%mm1      /* mm1 = tmp1[0] | tmp1[1] */
        / 0
        movq %mm1,%mm2
        movd 12(%ebx),%mm3     /* mm3 = tmp1[3] */
        punpckldq 8(%ebx),%mm3 /* mm3 = tmp1[3] | tmp1[2] */
        pfadd %mm3,%mm1        /* mm1 = tmp1[0]+tmp1[3] | tmp1[1]+tmp1[2]*/
        pfsub %mm3,%mm2        /* mm2 = tmp1[0]-tmp1[3] | tmp1[0]-tmp1[2]*/
        pfmul %mm0,%mm2        /* mm2 = tmp1[0]-tmp1[3]*pnts[3]|tmp1[0]-tmp1[2]*pnts[4]*/
        movq %mm1,0(%esi)      /* tmp2[0, 1] = mm1 */
        pswapd %mm2, %mm2      /* mm2 = tmp1[0]-tmp1[2]*pnts[4]|tmp1[0]-tmp1[3]*pnts[3] */
        movq   %mm2, 8(%esi)   /* tmp2[2, 3] = mm2 */
        movq 16(%ebx),%mm4
        / 4
        movq %mm4,%mm5
        movd 28(%ebx),%mm6
        punpckldq 24(%ebx),%mm6
        pfadd %mm6,%mm4
        pfsubr %mm6,%mm5
        pfmul %mm0,%mm5
        movq %mm4,16(%esi)
        pswapd %mm5, %mm5
        movq   %mm5, 24(%esi)
        movq 32(%ebx),%mm1
        / 8
        movq %mm1,%mm2
        movd 44(%ebx),%mm3
        punpckldq 40(%ebx),%mm3
        pfadd %mm3,%mm1
        pfsub %mm3,%mm2
        pfmul %mm0,%mm2
        movq %mm1,32(%esi)
        pswapd %mm2, %mm2
        movq   %mm2, 40(%esi)
        movq 48(%ebx),%mm4
        / 12
        movq %mm4,%mm5
        movd 60(%ebx),%mm6
        punpckldq 56(%ebx),%mm6
        pfadd %mm6,%mm4
        pfsubr %mm6,%mm5
        pfmul %mm0,%mm5
        movq %mm4,48(%esi)
        pswapd %mm5, %mm5
        movq   %mm5, 56(%esi)
        movq 64(%ebx),%mm1
        / 16
        movq %mm1,%mm2
        movd 76(%ebx),%mm3
        punpckldq 72(%ebx),%mm3
        pfadd %mm3,%mm1
        pfsub %mm3,%mm2
        pfmul %mm0,%mm2
        movq %mm1,64(%esi)
        pswapd %mm2, %mm2
        movq   %mm2, 72(%esi)
        movq 80(%ebx),%mm4
        / 20
        movq %mm4,%mm5
        movd 92(%ebx),%mm6
        punpckldq 88(%ebx),%mm6
        pfadd %mm6,%mm4
        pfsubr %mm6,%mm5
        pfmul %mm0,%mm5
        movq %mm4,80(%esi)
        pswapd %mm5, %mm5
        movq   %mm5, 88(%esi)
        movq 96(%ebx),%mm1
        / 24
        movq %mm1,%mm2
        movd 108(%ebx),%mm3
        punpckldq 104(%ebx),%mm3
        pfadd %mm3,%mm1
        pfsub %mm3,%mm2
        pfmul %mm0,%mm2
        movq %mm1,96(%esi)
        pswapd %mm2, %mm2
        movq   %mm2, 104(%esi)
        movq 112(%ebx),%mm4
        / 28
        movq %mm4,%mm5
        movd 124(%ebx),%mm6
        punpckldq 120(%ebx),%mm6
        pfadd %mm6,%mm4
        pfsubr %mm6,%mm5
        pfmul %mm0,%mm5
        movq %mm4,112(%esi)
        pswapd %mm5, %mm5
        movq   %mm5, 120(%esi)

        // 5
	movq plus_minus_3dnow, %mm0 /* mm0 = 1.0 | -1.0 */
        movl $1,%eax
        movd %eax,%mm1
        pi2fd %mm1,%mm1
        movl pnts+16,%eax
        movd 0(%eax),%mm2
        punpckldq %mm2,%mm1   /* mm1 = 1.0 | cos0 */
        movq 0(%esi),%mm2     /* mm2 = tmp2[0] | tmp2[1] */
        / 0
	pfpnacc %mm2, %mm2
	pswapd %mm2, %mm2     /* mm2 = tmp2[0]+tmp2[1]|tmp2[0]-tmp2[1]*/
        pfmul %mm1,%mm2       /* mm2 = tmp2[0]+tmp2[1]|(tmp2[0]-tmp2[1])*cos0*/
        movq %mm2,0(%ebx)     /* tmp1[0, 1] = mm2 */
        movq 8(%esi),%mm4     /* mm4 = tmp2[2] | tmp2[3]*/
	pfpnacc %mm4, %mm4
	pswapd  %mm4, %mm4    /* mm4 = tmp2[2]+tmp2[3]|tmp2[2]-tmp2[3]*/
        pxor  %mm0,%mm4       /* mm4 = tmp2[2]+tmp2[3]|tmp2[3]-tmp2[2]*/
        pfmul %mm1,%mm4       /* mm4 = tmp2[2]+tmp2[3]|(tmp2[3]-tmp2[2])*cos0*/
        movq %mm4,%mm5
        psrlq $32,%mm5        /* mm5 = (tmp2[3]-tmp2[2])*cos0 */
        pfacc %mm5,%mm4       /* mm4 = tmp2[2]+tmp2[3]+(tmp2[3]-tmp2[2])*cos0|(tmp2[3]-tmp2[2])*cos0*/
        movq %mm4,8(%ebx)     /* tmp1[2, 3] = mm4 */
        movq 16(%esi),%mm2
        / 4
	pfpnacc %mm2, %mm2
	pswapd %mm2, %mm2

        pfmul %mm1,%mm2
        movq 24(%esi),%mm4
	pfpnacc %mm4, %mm4
	pswapd  %mm4, %mm4

        pxor  %mm0,%mm4
        pfmul %mm1,%mm4
        movq %mm4,%mm5
        psrlq $32,%mm5
        pfacc %mm5,%mm4
        movq %mm2,%mm3
        psrlq $32,%mm3
        pfadd %mm4,%mm2
        pfadd %mm3,%mm4
        movq %mm2,16(%ebx)
        movq %mm4,24(%ebx)
        movq 32(%esi),%mm2
        / 8
	pfpnacc %mm2, %mm2
	pswapd %mm2, %mm2

        pfmul %mm1,%mm2
        movq %mm2,32(%ebx)
        movq 40(%esi),%mm4
	pfpnacc %mm4, %mm4
	pswapd  %mm4, %mm4
        pxor  %mm0,%mm4
        pfmul %mm1,%mm4
        movq %mm4,%mm5
        psrlq $32,%mm5
        pfacc %mm5,%mm4
        movq %mm4,40(%ebx)
        movq 48(%esi),%mm2
        / 12
	pfpnacc %mm2, %mm2
	pswapd %mm2, %mm2
        pfmul %mm1,%mm2
        movq 56(%esi),%mm4
	pfpnacc %mm4, %mm4
	pswapd  %mm4, %mm4
        pxor  %mm0,%mm4
        pfmul %mm1,%mm4
        movq %mm4,%mm5
        psrlq $32,%mm5
        pfacc %mm5,%mm4
        movq %mm2,%mm3
        psrlq $32,%mm3
        pfadd %mm4,%mm2
        pfadd %mm3,%mm4
        movq %mm2,48(%ebx)
        movq %mm4,56(%ebx)
        movq 64(%esi),%mm2
        / 16
	pfpnacc %mm2, %mm2
	pswapd %mm2, %mm2
        pfmul %mm1,%mm2
        movq %mm2,64(%ebx)
        movq 72(%esi),%mm4
	pfpnacc %mm4, %mm4
	pswapd  %mm4, %mm4
        pxor  %mm0,%mm4
        pfmul %mm1,%mm4
        movq %mm4,%mm5
        psrlq $32,%mm5
        pfacc %mm5,%mm4
        movq %mm4,72(%ebx)
        movq 80(%esi),%mm2
        / 20
	pfpnacc %mm2, %mm2
	pswapd %mm2, %mm2
        pfmul %mm1,%mm2
        movq 88(%esi),%mm4
	pfpnacc %mm4, %mm4
	pswapd  %mm4, %mm4
        pxor  %mm0,%mm4
        pfmul %mm1,%mm4
        movq %mm4,%mm5
        psrlq $32,%mm5
        pfacc %mm5,%mm4
        movq %mm2,%mm3
        psrlq $32,%mm3
        pfadd %mm4,%mm2
        pfadd %mm3,%mm4
        movq %mm2,80(%ebx)
        movq %mm4,88(%ebx)
        movq 96(%esi),%mm2
        / 24
	pfpnacc %mm2, %mm2
	pswapd %mm2, %mm2
        pfmul %mm1,%mm2
        movq %mm2,96(%ebx)
        movq 104(%esi),%mm4
	pfpnacc %mm4, %mm4
	pswapd  %mm4, %mm4
        pxor  %mm0,%mm4
        pfmul %mm1,%mm4
        movq %mm4,%mm5
        psrlq $32,%mm5
        pfacc %mm5,%mm4
        movq %mm4,104(%ebx)
        movq 112(%esi),%mm2
        / 28
	pfpnacc %mm2, %mm2
	pswapd %mm2, %mm2
        pfmul %mm1,%mm2
        movq 120(%esi),%mm4
	pfpnacc %mm4, %mm4
	pswapd  %mm4, %mm4
        pxor  %mm0,%mm4
        pfmul %mm1,%mm4
        movq %mm4,%mm5
        psrlq $32,%mm5
        pfacc %mm5,%mm4
        movq %mm2,%mm3
        psrlq $32,%mm3
        pfadd %mm4,%mm2
        pfadd %mm3,%mm4
        movq %mm2,112(%ebx)
        movq %mm4,120(%ebx)

        // Phase6
        movd 0(%ebx),%mm0
        movd %mm0,1024(%ebp)
        movl 4(%ebx),%eax
        movl %eax,0(%ebp)
        movl %eax,0(%edx)
        movd 8(%ebx),%mm2
        movd %mm2,512(%ebp)
        movd 12(%ebx),%mm3
        movd %mm3,512(%edx)

        movl 16(%ebx),%eax
        movl %eax,768(%ebp)
        movd 20(%ebx),%mm5
        movd %mm5,256(%edx)

        movd 24(%ebx),%mm6
        movd %mm6,256(%ebp)
        movd 28(%ebx),%mm7
        movd %mm7,768(%edx)

        movq 32(%ebx),%mm0       /* mm0 = tmp1[8] | tmp1[9] */
        movq 48(%ebx),%mm1       /* mm1 = tmp1[12] | tmp1[13] */
        pfadd %mm1,%mm0          /* mm0 = tmp1[8]+tmp1[12]| tmp1[9]+tmp1[13]*/
        movd %mm0,896(%ebp)      /* a[0xE0] = tmp1[8]+tmp1[12] */
        psrlq $32,%mm0
        movd %mm0,128(%edx)      /* a[0x20] = tmp1[9]+tmp1[13] */
        movq 40(%ebx),%mm2
        pfadd %mm2,%mm1
        movd %mm1,640(%ebp)
        psrlq $32,%mm1
        movd %mm1,384(%edx)

        movq 56(%ebx),%mm3
        pfadd %mm3,%mm2
        movd %mm2,384(%ebp)
        psrlq $32,%mm2
        movd %mm2,640(%edx)

        movd 36(%ebx),%mm4
        pfadd %mm4,%mm3
        movd %mm3,128(%ebp)
        psrlq $32,%mm3
        movd %mm3,896(%edx)
        movq 96(%ebx),%mm0
        movq 64(%ebx),%mm1

        movq 112(%ebx),%mm2
        pfadd %mm2,%mm0
        movq %mm0,%mm3
        pfadd %mm1,%mm3
        movd %mm3,960(%ebp)
        psrlq $32,%mm3
        movd %mm3,64(%edx)
        movq 80(%ebx),%mm1
        pfadd %mm1,%mm0
        movd %mm0,832(%ebp)
        psrlq $32,%mm0
        movd %mm0,192(%edx)
        movq 104(%ebx),%mm3
        pfadd %mm3,%mm2
        movq %mm2,%mm4
        pfadd %mm1,%mm4
        movd %mm4,704(%ebp)
        psrlq $32,%mm4
        movd %mm4,320(%edx)
        movq 72(%ebx),%mm1
        pfadd %mm1,%mm2
        movd %mm2,576(%ebp)
        psrlq $32,%mm2
        movd %mm2,448(%edx)

        movq 120(%ebx),%mm4
        pfadd %mm4,%mm3
        movq %mm3,%mm5
        pfadd %mm1,%mm5
        movd %mm5,448(%ebp)
        psrlq $32,%mm5
        movd %mm5,576(%edx)
        movq 88(%ebx),%mm1
        pfadd %mm1,%mm3
        movd %mm3,320(%ebp)
        psrlq $32,%mm3
        movd %mm3,704(%edx)

        movd 100(%ebx),%mm5
        pfadd %mm5,%mm4
        movq %mm4,%mm6
        pfadd %mm1,%mm6
        movd %mm6,192(%ebp)
        psrlq $32,%mm6
        movd %mm6,832(%edx)
        movd 68(%ebx),%mm1
        pfadd %mm1,%mm4
        movd %mm4,64(%ebp)
        psrlq $32,%mm4
        movd %mm4,960(%edx)

        / femms

        popl %ebx
        popl %esi
        popl %edi
        popl %ebp
        addl $256,%esp

        ret  $12

