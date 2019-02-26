.data
/* sys.__cenvp : byte## */
.globl sys$__cenvp
sys$__cenvp:
    .quad 0

.globl thread$__tls
thread$__tls:
    .fill 88 /* sizeof(tlshdr) + (8 * sizeof(void#)) = 24 + 64 */

.text
/*
 * The entry point for the whole program.
 * This is called by the OS. In order, it:
 *  - Sets up all argc entries as slices
 *  - Converts argc/argv to a slice
 *  - Stashes a raw envp copy in __cenvp (for syscalls to use)
 *  - Sets up thread local storage for the main thread
 *  - Calls main()
 */
.globl _start
_start:
	movq	%rsp,%rbp
	andq	$-16,%rsp		/* align the stack pointer */

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

	/* set up the intial tls region for the main thread */
	movq	$158,%rax		/* arch_prctl */
	movq	$0x1002,%rdi		/* Archsetfs */
	leaq	thread$__tls(%rip),%rsi
	syscall

	xorq %rbp,%rbp
	call	__init__
	call	main
	call	__fini__

	/* exit(0) */
	xorq	%rdi,%rdi
	movq	$60,%rax
	syscall
