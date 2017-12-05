.data
/* sys.__cenvp : byte## */
.globl sys$__cenvp
sys$__cenvp:
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
.globl _start
_start:
	andq	$-16,%rsp		/* align the stack pointer */

	/* load argc, argv, envp from stack */
	movq	(%rdi),%rax		/* argc */
	leaq	8(%rdi),%rbx		/* argv */
	leaq	16(%rdi,%rax,8),%rcx	/* envp = argv + 8*argc + 8 */

	/* store envp for some syscalls to use without converting */
	movq    %rcx,sys$__cenvp(%rip)

	/* stack allocate sizeof(byte[:])*argc */
	imulq	$16,%rax,%rdx
	subq	%rdx,%rsp
	movq	%rsp,%rcx		/* args[:] */

	/* convert argc, argv to byte[:][:] for args. */
	pushq	%rax
	pushq	%rcx
	call	cvt

	xorq %rbp,%rbp
	/* call pre-main initializers */
	call	__init__

	/* enter the main program */
	call	main
	/* exit(0) */
	xorq	%rdi,%rdi
	movq	$1,%rax
	syscall
