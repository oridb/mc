.section ".note.openbsd.ident", "a"
	.p2align 2
	.long   8
	.long   4
	.long   1
	.ascii "OpenBSD\0"
	.long   0
	.p2align 2

.data

/* sys._environment : byte[:][:] */
.globl sys$__environment
sys$__environment:
    .envbase:
    .quad 0 /* env size */
    .envlen:
    .quad 0 /* env ptr */

.globl sys$__cenvp
sys$__cenvp:
    .quad 0

.text
/*
 * The entry point for the whole program.
 * This is called by the OS. In order, it:
 *  - Sets up all argc entries as slices
 *  - Sets up all envp entries as slices
 *  - Converts argc/argv to a slice
 *  - Stashes envp in sys._environment
 *  - Stashes a raw envp copy in __cenvp (for syscalls to use)
 *  - Calls main()
 */
.globl _start
_start:
	/* turn args into a slice */
	movq	%rsp,%rbp

	/* stack allocate sizeof(byte[:])*(argc + len(envp)) */
	movq	(%rbp),%rax
	leaq	16(%rbp,%rax,8), %rbx	/* argp = argv + 8*argc + 8 */
	call    count
	addq	%r9,%rax
	imulq	$16,%rax
	subq	%rax,%rsp
	movq	%rsp, %rdx	/* saved args[:] */

	/* stack allocate sizeof(byte[:])*(argc + len(envp)) */
	movq	(%rbp),%rax
	leaq	16(%rbp,%rax,8), %rbx	/* argp = argv + 8*argc + 8 */
	call    count
	addq	%r9,%rax
	imulq	$16,%rax
	subq	%rax,%rsp
	movq	%rsp, %rdx	/* saved args[:] */

	/* convert envp to byte[:][:] for sys._environment */
	movq	(%rbp),%rax
	leaq	16(%rbp,%rax,8), %rbx	/* envp = argv + 8*argc + 8 */
	/* store envp for some syscalls to use without converting */
	movq    %rbx,sys$__cenvp(%rip)
	movq	%r9,%rax
	movq	%rsp, %rcx
	movq	%r9,.envlen(%rip)
	movq	%rdx,.envbase(%rip)
	call	cvt
	movq	%rcx,%rdx
	
	/* convert argc, argv to byte[:][:] for args. */
	movq	(%rbp), %rax	/* argc */
	leaq	8(%rbp), %rbx	/* argv */
	movq	(%rbp), %rsi	/* saved argc */
	call	cvt
	pushq	%rsi
	pushq	%rdx

	/* call pre-main initializers */
	call	__init__
	/* enter the main program */
	call	main
	/* exit(0) */
	xorq	%rdi,%rdi
	movq	$1,%rax
	syscall

