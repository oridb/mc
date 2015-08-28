.globl thread$clone
thread$clone:
	pushq %r15
	movq 16(%rsp),%r15
	/* clone(flags, stack, ptid, tls, ctid, regs) */
	movq $56,%rax	/* syscall num */
	/* %rdi: flags */
	/* %rsi: stack */
	/* %rdx: ptid */
	movq %rcx,%r10	/* tls */
	/* %r8: ctid */
	/* %r9: regs */
	syscall
	
	/* fn() */
	testl %eax,%eax
	jnz parent
	call *%r15
	
	/* exit(0) */
	movq $60, %rax  /* exit */
	movq $0, %rdi   /* arg: 0 */
	syscall

parent:
	popq %r15
	ret
