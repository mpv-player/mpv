.section .data
.globl caller_return
caller_return:
	.long 0
.globl report_entry
report_entry:
	.long null_call
.globl report_ret
report_ret:	
	.long null_call
.global wrapper_target
wrapper_target:	
	.long null_call

.section .text
.globl null_call
	.type null_call, @function
	.balign 16,0x90
null_call:
	ret
.globl wrapper
	.type wrapper, @function
	.balign 16,0x90
wrapper:
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
	
	call *report_entry	# report entry
	
	test %eax, %eax
	jnz .Ldone

	leave			# restore %esp, %ebp
	popf			# restore flags
	popa			# restore registers
	
	popl caller_return	# switch return addresses
	pushl $.Lwrapper_return 
	
	jmp *wrapper_target	# wrapper_target should return at .Lwrapper_return

	.balign 16, 0x90
.Lwrapper_return:	
	pushl caller_return	# restore the original return address
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

	call *report_ret	# report the return information (same args)
.Ldone:	

	leave
	popf
	popa
	ret
	
