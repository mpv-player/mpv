# This code was taken from http://www.mpg123.org
# See ChangeLog of mpg123-0.59s-pre.1 for detail
# Applied to mplayer by Nick Kurshev <nickols_k@mail.ru>

.data
	.align 4
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

.globl dct64_MMX
dct64_MMX:
	pushl %ebx
	pushl %esi
	pushl %edi
	subl $256,%esp
	movl 280(%esp),%eax
/* Phase 1*/
	flds     (%eax)
	leal 128(%esp),%edx
	fadds 124(%eax)
	movl 272(%esp),%esi
	fstps    (%edx)
	movl 276(%esp),%edi

	flds    4(%eax)
	movl $costab,%ebx
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
	
/* Phase 2*/

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
	
/* Phase 3*/

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
	ret
	

