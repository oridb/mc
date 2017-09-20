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
	/*
	if there syscalls are more than 6
	args (eg, mmap), the remaining args
	are on the stack, with 8 dummy bytes 
	for a return address.
	*/
	movq 64(%rsp),%rbx
	pushq %rbx
	pushq %rbx

	syscall
	jae .success
	negq %rax

.success:
	addq $16,%rsp
	ret

.globl sys$__netbsd_pipe
sys$__netbsd_pipe:
	movq $0x2a,%rax
	syscall

	jae .pipesuccess
	negq %rax

.pipesuccess:
	movl %eax,(%rdi)
	movl %edx,4(%rdi)
	xorq %rax,%rax
	ret

/* __tfork_thread(tfp : tforkparams#, sz : size, fn : void#, arg : void#-> tid) */
.globl sys$__tfork_thread
sys$__tfork_thread:
	/* syscall */
	movq	%rdx, %r8
	movq	%rcx, %r9
	movq	$8, %rax
	syscall
	jb	.failparent

	/* are we in the parent? */
	cmpq	$0,%rax
	jnz	.doneparent

	/* call the function and __threxit */
	movq	%r9, %rdi
	callq	*%r8

	movq	$302,%rax
	xorq	%rdi,%rdi
	syscall

.failparent:
	negq	%rax

.doneparent:
	ret
