#if defined(__APPLE__)
# define SYM(x) _ ## x
#else
# define SYM(x) x
#endif
	.data
.globl SYM(exp_EH_prolog)
SYM(exp_EH_prolog):
	pushl $0xff
	pushl %eax
	pushl %fs:0
	movl  %esp, %fs:0
	movl  12(%esp), %eax
	movl  %ebp, 12(%esp)
	leal  12(%esp), %ebp
	pushl %eax
	ret
