	.file	"stubs.c"
	.version	"01.01"
gcc2_compiled.:
.section	.rodata
.LC0:
	.string	"Called unk_%s\n"
.text
	.align 4
.globl unk_exp1
	.type	 unk_exp1,@function
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
	jmp .L1
	.align 4
.L1:
	leave
	ret
.Lfe1:
	.size	 unk_exp1,.Lfe1-unk_exp1
	.ident	"GCC: (GNU) egcs-2.91.66 19990314/Linux (egcs-1.1.2 release)"
