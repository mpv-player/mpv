	.data
.LC0:	.string	"Called unk_%s\n"
       .balign 4
.globl unk_exp1
unk_exp1:
	pushl %ebp
	movl %esp,%ebp
	subl $4,%esp
	movl $1,-4(%ebp)
	movl -4(%ebp),%eax
	movl %eax,%ecx
	movl %ecx,%edx
	sall $4,%edx
	subl %eax,%edx
	leal 0(,%edx,2),%eax
	movl %eax,%edx
	addl $export_names,%edx
	pushl %edx
	pushl $.LC0
	call printf
	addl $8,%esp
	xorl %eax,%eax
	leave
	ret
.globl exp_EH_prolog
exp_EH_prolog:
	pushl $0xff
	pushl %eax
	pushl %fs:0
	movl  %esp, %fs:0
	movl  12(%esp), %eax
	movl  %ebp, 12(%esp)
	leal  12(%esp), %ebp
	pushl %eax
	ret
