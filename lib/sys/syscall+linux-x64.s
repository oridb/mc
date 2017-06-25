.globl sys$syscall
sys$syscall:
	/*
	hack: We load 6 args regardless of
	how many we actually have. This may
	load junk values, but if the syscall
	doesn't use them, it's going to be
	harmless.
	 */
	movq %rdi,%rax
        /* 8(%rsp): hidden type arg */
	movq 16(%rsp),%rdi
	movq 24(%rsp),%rsi
	movq 32(%rsp),%rdx
	movq 40(%rsp),%r10
	movq 48(%rsp),%r8
	movq 56(%rsp),%r9

	syscall

	ret

/* clone(flags, stack, ptid, tls, ctid, regs) */
.globl sys$fnclone
sys$fnclone:
	pushq %r15
        /* %rsp is modified by clone(), so it's saved here */
	movq 16(%rsp),%r15
	/*
        %rdi: flags, %rsi: stack, %rdx: ptid,
        %rcx: tls, %r8: ctid, %r9: regs
        */
	movq $56,%rax	/* syscall num */
	movq %rcx,%r10	/* tls */
	syscall
	cmpq $0,%rax
	jb .doneparent

	/* fn() */
	testl %eax,%eax
	jnz .doneparent
	call *%r15

	/* exit(0) */
	movq $60, %rax  /* exit */
	movq $0, %rdi   /* arg: 0 */
	syscall

.doneparent:
	popq %r15
	ret

.globl sys$sigreturn
sys$sigreturn:
	movq $15,%rax
	syscall
