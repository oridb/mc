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
	jae .success
	negq %rax

.success:
	ret

/*
 * pipe() syscall returns the pipes created in eax:edx, and
 * needs to copy them to the destination locations manually.
 */
.globl sys$__freebsd_pipe
sys$__freebsd_pipe:
	movq $0x2a,%rax
	syscall

	jae .pipesuccess
	negq %rax

.pipesuccess:
	movl %eax,(%rdi)
	movl %edx,4(%rdi)
	xorq %rax,%rax
	ret

