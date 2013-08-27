.data
/* std._environment : byte[:][:] */
.globl _std$_environment
_std$_environment:
.envbase:
.quad 0 /* env size */
.envlen:
.quad 0 /* env ptr */

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
 * Counts the size of the null terminated string vector
 * pointed to by %rbx. Clobbers %r10,%r11
 */
count:
	xorq %r9,%r9
	movq %rbx,%r11
.countloop:
	movq (%r11),%r10
	testq %r10,%r10
	jz .countdone
	addq $1,%r9
	addq $8,%r11
	jmp .countloop
.countdone:
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

.globl start
start:
	/* turn args into a slice */
	movq	%rsp,%rbp
	/* stack allocate sizeof(byte[:])*(argc + len(envp)) */
	movq	(%rbp),%rax
	leaq	16(%rbp,%rax,8), %rbx	/* envp = argv + 8*argc + 8 */
        call    count
	addq	%r9,%rax
	imulq	$16,%rax
	subq	%rax,%rsp
	movq	%rsp, %rdx	/* saved args[:] */

	/* convert envp to byte[:][:] for std._environment */
	movq	(%rbp),%rax
	leaq	16(%rbp,%rax,8), %rbx	/* envp = argv + 8*argc + 8 */
	movq	%r9,%rax
	movq	%rsp, %rcx
	movq	%r9,.envlen(%rip)
	movq	%rdx,.envbase(%rip)
	call cvt
	movq	%rcx,%rdx

        /* convert argc, argv to byte[:][:] for args. */
	movq	(%rbp), %rax	/* argc */
	leaq	8(%rbp), %rbx	/* argv */
	movq	(%rbp), %rsi	/* saved argc */
        call cvt
	pushq %rsi
	pushq %rdx

	/* enter the main program */
	call	_main
	/* exit */
	movq	%rax,%rdi
	movq	$0x2000001,%rax
	syscall

