.globl std$setjmp
.globl _std$setjmp
std$setjmp:
_std$setjmp:
	/* save registers */
	movq	%rbx,   (%rdi)
	movq	%rbp,  8(%rdi)
	movq	%r12, 16(%rdi)
	movq	%r13, 24(%rdi)
	movq	%r14, 32(%rdi)
	movq	%r15, 40(%rdi)

	/* save %rsp */
	leaq	8(%rsp), %rdx
	movq	%rdx, 48(%rdi)

	/* save %rip */
	movq	(%rsp), %rdx
	movq	%rdx, 56(%rdi)
	xorq	%rax, %rax
	ret	

.globl std$longjmp
.globl _std$longjmp
std$longjmp:
_std$longjmp:
	/* return true */
	movq	$1, %rax

	/* restore registers */
	movq	  (%rdi), %rbx
	movq	 8(%rdi), %rbp
	movq	16(%rdi), %r12
	movq	24(%rdi), %r13
	movq	32(%rdi), %r14
	movq	40(%rdi), %r15
	movq	48(%rdi), %rdx

	/* load stack */
	movq	%rdx, %rsp

	/* jmp to return addr */
	movq	56(%rdi), %rdx
	jmp	*%rdx
