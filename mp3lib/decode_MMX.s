# this code comes under GPL
# This code was taken from http://www.mpg123.org
# See ChangeLog of mpg123-0.59s-pre.1 for detail
# Applied to mplayer by Nick Kurshev <nickols_k@mail.ru>
#
# Local ChangeLog:
# - Partial loops unrolling and removing MOVW insn from loops
#

.data
.align 8
null_one: .long 0x0000ffff, 0x0000ffff
one_null: .long 0xffff0000, 0xffff0000
.globl costab_mmx
costab_mmx:
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

.globl synth_1to1_MMX_s
//
// void synth_1to1_MMX_s(real *bandPtr, int channel, short *samples,
//                       short *buffs, int *bo);
//
synth_1to1_MMX_s:
        pushl %ebp
        pushl %edi
        pushl %esi
        pushl %ebx
        movl 24(%esp),%ecx
        movl 28(%esp),%edi
        movl $15,%ebx
        movl 36(%esp),%edx
        leal (%edi,%ecx,2),%edi
	decl %ecx
        movl 32(%esp),%esi
        movl (%edx),%eax
        jecxz .L1
        decl %eax
        andl %ebx,%eax
        leal 1088(%esi),%esi
        movl %eax,(%edx)
.L1:
        leal (%esi,%eax,2),%edx
        movl %eax,%ebp
        incl %eax
        pushl 20(%esp)
        andl %ebx,%eax
        leal 544(%esi,%eax,2),%ecx
        incl %ebx
	testl $1, %eax
	jnz .L2
        xchgl %edx,%ecx
	incl %ebp
        leal 544(%esi),%esi
.L2: 
	emms
        pushl %edx
        pushl %ecx
        call *dct64_MMX_func
	leal 1(%ebx), %ecx
        subl %ebp,%ebx
	pushl %ecx
	leal decwins(%ebx,%ebx,1), %edx
	shrl $1, %ecx
.align 16
.L3: 
        movq  (%edx),%mm0
        movq  64(%edx),%mm4
        pmaddwd (%esi),%mm0
        pmaddwd 32(%esi),%mm4
        movq  8(%edx),%mm1
        movq  72(%edx),%mm5
        pmaddwd 8(%esi),%mm1
        pmaddwd 40(%esi),%mm5
        movq  16(%edx),%mm2
        movq  80(%edx),%mm6
        pmaddwd 16(%esi),%mm2
        pmaddwd 48(%esi),%mm6
        movq  24(%edx),%mm3
        movq  88(%edx),%mm7
        pmaddwd 24(%esi),%mm3
        pmaddwd 56(%esi),%mm7
        paddd %mm1,%mm0
        paddd %mm5,%mm4
        paddd %mm2,%mm0
        paddd %mm6,%mm4
        paddd %mm3,%mm0
        paddd %mm7,%mm4
        movq  %mm0,%mm1
        movq  %mm4,%mm5
        psrlq $32,%mm1
        psrlq $32,%mm5
        paddd %mm1,%mm0
        paddd %mm5,%mm4
        psrad $13,%mm0
        psrad $13,%mm4
        packssdw %mm0,%mm0
        packssdw %mm4,%mm4

	movq	(%edi), %mm1
	punpckldq %mm4, %mm0
	pand   one_null, %mm1
	pand   null_one, %mm0
	por    %mm0, %mm1
	movq   %mm1,(%edi)

        leal 64(%esi),%esi
        leal 128(%edx),%edx
        leal 8(%edi),%edi                

	decl %ecx
        jnz  .L3

	popl %ecx
	andl $1, %ecx
	jecxz .next_loop

        movq  (%edx),%mm0
        pmaddwd (%esi),%mm0
        movq  8(%edx),%mm1
        pmaddwd 8(%esi),%mm1
        movq  16(%edx),%mm2
        pmaddwd 16(%esi),%mm2
        movq  24(%edx),%mm3
        pmaddwd 24(%esi),%mm3
        paddd %mm1,%mm0
        paddd %mm2,%mm0
        paddd %mm3,%mm0
        movq  %mm0,%mm1
        psrlq $32,%mm1
        paddd %mm1,%mm0
        psrad $13,%mm0
        packssdw %mm0,%mm0
        movd %mm0,%eax
	movw %ax, (%edi)
        leal 32(%esi),%esi
        leal 64(%edx),%edx
        leal 4(%edi),%edi                
	
.next_loop:
        subl $64,%esi                    
        movl $7,%ecx
.align 16
.L4: 
        movq  (%edx),%mm0
        movq  64(%edx),%mm4
        pmaddwd (%esi),%mm0
        pmaddwd -32(%esi),%mm4
        movq  8(%edx),%mm1
        movq  72(%edx),%mm5
        pmaddwd 8(%esi),%mm1
        pmaddwd -24(%esi),%mm5
        movq  16(%edx),%mm2
        movq  80(%edx),%mm6
        pmaddwd 16(%esi),%mm2
        pmaddwd -16(%esi),%mm6
        movq  24(%edx),%mm3
        movq  88(%edx),%mm7
        pmaddwd 24(%esi),%mm3
        pmaddwd -8(%esi),%mm7
        paddd %mm1,%mm0
        paddd %mm5,%mm4
        paddd %mm2,%mm0
        paddd %mm6,%mm4
        paddd %mm3,%mm0
        paddd %mm7,%mm4
        movq  %mm0,%mm1
        movq  %mm4,%mm5
        psrlq $32,%mm1
        psrlq $32,%mm5
        paddd %mm0,%mm1
        paddd %mm4,%mm5
        psrad $13,%mm1
        psrad $13,%mm5
        packssdw %mm1,%mm1
        packssdw %mm5,%mm5
        psubd %mm0,%mm0
        psubd %mm4,%mm4
        psubsw %mm1,%mm0
        psubsw %mm5,%mm4

	movq	(%edi), %mm1
	punpckldq %mm4, %mm0
	pand   one_null, %mm1
	pand   null_one, %mm0
	por    %mm0, %mm1
	movq   %mm1,(%edi)

        subl $64,%esi
        addl $128,%edx
        leal 8(%edi),%edi                
        decl %ecx
	jnz  .L4

        movq  (%edx),%mm0
        pmaddwd (%esi),%mm0
        movq  8(%edx),%mm1
        pmaddwd 8(%esi),%mm1
        movq  16(%edx),%mm2
        pmaddwd 16(%esi),%mm2
        movq  24(%edx),%mm3
        pmaddwd 24(%esi),%mm3
        paddd %mm1,%mm0
        paddd %mm2,%mm0
        paddd %mm3,%mm0
        movq  %mm0,%mm1
        psrlq $32,%mm1
        paddd %mm0,%mm1
        psrad $13,%mm1
        packssdw %mm1,%mm1
        psubd %mm0,%mm0
        psubsw %mm1,%mm0
        movd %mm0,%eax
	movw %ax,(%edi)

	emms
        popl %ebx
        popl %esi
        popl %edi
        popl %ebp
        ret
