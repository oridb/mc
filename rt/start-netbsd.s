.section .note.netbsd.ident
        .long	7
	.long	4
	.long	1
	.ascii "NetBSD\0"
	.p2align 2
        .long      200000000

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
	movq	%rsp,%rbp

	/* load argc, argv, envp from stack */
	movq	(%rbp),%rax		/* argc */
	leaq	8(%rbp),%rbx		/* argv */
	leaq	16(%rbp,%rax,8),%rcx	/* envp = argv + 8*argc + 8 */

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

