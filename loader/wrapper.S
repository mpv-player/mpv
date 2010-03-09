#include "config.h"

#define GLUE(a, b) a ## b
#define JOIN(a, b) GLUE(a, b)
#define MANGLE(s)  JOIN(EXTERN_ASM, s)

.data
.globl MANGLE(caller_return)
MANGLE(caller_return):
	.long 0
.globl MANGLE(report_entry)
MANGLE(report_entry):
	.long MANGLE(null_call)
.globl MANGLE(report_ret)
MANGLE(report_ret):
	.long MANGLE(null_call)
.global MANGLE(wrapper_target)
MANGLE(wrapper_target):
	.long MANGLE(null_call)

.text
.globl MANGLE(null_call)
	.type MANGLE(null_call), @function
	.balign 16,0x90
MANGLE(null_call):
	ret
.globl MANGLE(wrapper)
	.type MANGLE(wrapper), @function
	.balign 16,0x90
MANGLE(wrapper):
	pusha			# store registers (EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI)
	pushf			# store flags

	push %ebp		# set up a stack frame
	movl %esp, %ebp

	leal 4(%ebp), %eax	# push flags addr
	push %eax
	leal 8(%ebp), %eax	# push registers addr
	push %eax

	leal 40(%ebp), %edx
	movl (%ebp), %eax
	subl %edx, %eax
	push %eax
	push %edx

	call *MANGLE(report_entry)	# report entry

	test %eax, %eax
	jnz .Ldone

	leave			# restore %esp, %ebp
	popf			# restore flags
	popa			# restore registers

	popl MANGLE(caller_return)	# switch return addresses
	pushl $.Lwrapper_return

	jmp *MANGLE(wrapper_target)	# wrapper_target should return at .Lwrapper_return

	.balign 16, 0x90
.Lwrapper_return:
	pushl MANGLE(caller_return)	# restore the original return address
	pusha			# more for reference sake here
	pushf

	push %ebp		# set up a stack frame
	movl %esp, %ebp

	leal 4(%ebp), %eax	# push flags addr
	push %eax
	leal 8(%ebp), %eax	# push registers addr
	push %eax

	leal 40(%ebp), %edx	# push stack top address (relative to our entry)
	movl (%ebp), %eax
	subl %edx, %eax		# calculate difference between entry and previous frame
	push %eax
	push %edx

	call *MANGLE(report_ret)	# report the return information (same args)
.Ldone:

	leave
	popf
	popa
	ret
