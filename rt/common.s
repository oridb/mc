.text
/*
 * counts the length of the string pointed to
 * by %r8, returning len in %r9. Does not modify
 * any registers outside of %r9
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

/*
 * iterate over the strings for argc, and put
 * them into the args array.
 * 
 * argc in %rax, argv in %rbx, dest vector in %rcx
 */
cvt:
	jmp .cvttest
.cvtloop:
	subq	$1,%rax
	movq	(%rbx),%r8
	call	cstrlen
	movq	%r8, (%rcx)
	movq	%r9, 8(%rcx)
	addq	$8, %rbx
	addq	$16, %rcx
.cvttest:
	testq	%rax,%rax
	jnz .cvtloop
.cvtdone:
	ret
