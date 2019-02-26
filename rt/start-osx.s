.data
/* sys.__cenvp : byte## */
.globl _sys$__cenvp
_sys$__cenvp:
    .quad 0

.globl thread$__tls
thread$__tls:
    .fill 104 /* sizeof(tlshdr) + (8 * sizeof(void#)) = 40 + 64 */

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
.globl start
start:
	movq	%rsp,%rbp
	andq	$-16,%rsp		/* align the stack pointer */

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

	/* set up the intial tls region for the main thread */
	movq	$0x3000003,%rax		/* undocumented setgsbase syscall */
	leaq	thread$__tls(%rip),%rdi
	movq	%rdi,0x20(%rdi)		/* also store a copy in __tls.self */
	syscall

	xorq %rbp,%rbp

	call	___init__
	call	_main
	call	___fini__

	/* exit */
	xorq	%rdi,%rdi
	movq	$0x2000001,%rax
	syscall

