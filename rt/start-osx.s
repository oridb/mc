.data
/* sys.__cenvp : byte## */
.globl _sys$__cenvp
_sys$__cenvp:
    .quad 0

.text
/*
 * The entry point for the whole program.
 * This is called by the OS. In order, it:
 *  - Sets up all argc entries as slices
 *  - Converts argc/argv to a slice
 *  - Stashes a raw envp copy in __cenvp (for syscalls to use)
 *  - Calls main()
 */
.globl start
start:
	movq	%rsp,%rbp

	/* load argc, argv, envp from stack */
	movq	(%rbp),%rax		/* argc */
	leaq	8(%rbp),%rbx		/* argv */
	leaq	16(%rbp,%rax,8),%rcx	/* envp = argv + 8*argc + 8 */

	/* store envp for some syscalls to use without converting */
	movq    %rcx,_sys$__cenvp(%rip)

	/* stack allocate sizeof(byte[:])*argc */
	imulq	$16,%rax,%rdx
	subq	%rdx,%rsp
	movq	%rsp,%rcx		/* args[:] */

	/* convert argc, argv to byte[:][:] for args. */
	pushq	%rax
	pushq	%rcx
	call	cvt

	xorq %rbp,%rbp
	call	___init__
	/* enter the main program */
	call	_main
	/* exit */
	xorq	%rdi,%rdi
	movq	$0x2000001,%rax
	syscall

