# this code comes under GPL
# This code was taken from http://www.mpg123.org
# See ChangeLog of mpg123-0.59s-pre.1 for detail
# Applied to mplayer by Nick Kurshev <nickols_k@mail.ru>
#
# TODO: Partial loops unrolling and removing MOVW insn.
#

.text

.globl synth_1to1_MMX_s

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
        addl $12,%esp
	leal 1(%ebx), %ecx
        subl %ebp,%ebx                

	leal decwins(%ebx,%ebx,1), %edx
.L3: 
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
	decl %ecx
        jnz  .L3


        subl $64,%esi                    
        movl $15,%ecx
.L4: 
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

        subl $32,%esi
        addl $64,%edx
        leal 4(%edi),%edi                
        decl %ecx
	jnz  .L4
	emms
        popl %ebx
        popl %esi
        popl %edi
        popl %ebp
        ret


