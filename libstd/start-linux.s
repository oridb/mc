/*
 * counts the length of the string pointed to
 * by %rbx, returning len in %r9
 */
cstrlen:
	xorq	%r9,%r9
	jmp .lentest

	.lenloop:
	incq	%r9
	.lentest:
	cmpb	$0,(%r8,%r9)
	jne	.lenloop
	ret

.globl _start
_start:
	/* turn args into a slice */
	movq	%rsp,%rbp
	/* stack allocate sizeof(byte[:])*argc */
	movq	(%rbp),%rax
	imulq	$16,%rax
	subq	%rax,%rsp
	movq	%rsp, %rdx	/* saved args[:] */

	/* iterate over the strings for argc, and put
	 * them into the args array. */
	movq	(%rbp), %rax	/* argc */
	leaq	8(%rbp), %rbx	/* argv */
	movq	%rsp, %rcx
	movq	(%rbp), %rsi	/* saved argc */

	testq	%rax,%rax
	jz .cvtdone
	.cvtloop:
	subq	$1,%rax
	movq	(%rbx),%r8
	call	cstrlen
	movq	%r8, (%rcx)
	movq	%r9, 8(%rcx)
	addq	$8, %rbx
	addq	$16, %rcx
	testq	%rax,%rax
	jnz .cvtloop
	.cvtdone:
	pushq %rsi
	pushq %rdx

	/* enter the main program */
	call	main
	/* exit */
	movq	%rax,%rdi
	movq	$60,%rax
	syscall

