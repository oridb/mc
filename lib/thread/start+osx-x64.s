# The entry point for thread start, registered with bsdthread_register
#      %rdi: pthread (0, for us)
#      %rsi: mach thread port (ignored)
#      %rdx: func
#      %rcx: env
#      %r8: stack
#      %r9: flags (= 0)
#      %rsp: stack - C_64_REDZONE_LEN (= stack - 128)
.globl _thread$start
_thread$start:
	/* call the function */
	movq	%r8, %rsp	/* set up stack */
	movq	%rcx,%rdi
        callq    *%rdx		/* call function */
	
/*
const thread.exit	: (stacksz : std.size -> void)
NOTE: must be called from the bottom of the stack, since
we assume that %rbp is in the top 4k of the stack.
*/
.globl _thread$exit
_thread$exit:
	/* find top of stack */
	movq	%rbp,%rdi	/* addr */
	andq	$~0xfff,%rdi	/* align it */
	addq	$0x1000,%rdi

	/* munmap(base, size) */
	movq	$0x2000049,%rax	/* munmap */
	movq	-8(%rdi),%rsi	/* size */
	subq	%rsi,%rdi	/* move to base ptr */
	syscall

	/* exit the thread */
	movq	$0x2000169, %rax	/* Sysbsdthread_terminate */
	movq	$0, %rdi	/* stack */
	movq	$0, %rsi	/* len */
	movq	$0, %rdx	/* sem */
	syscall
	

