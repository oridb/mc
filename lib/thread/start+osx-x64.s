// The entry point for thread start, registered with bsdthread_register
//      %rdi: pthread (0, for us)
//      %rsi: mach thread port (ignored)
//      %rdx: func
//      %rcx: env
//      %r8: stack
//      %r9: flags (= 0)
//      %rsp: stack - C_64_REDZONE_LEN (= stack - 128)
.globl _thread$start
_thread$start:
	/* call the function */
#	movq	%r8, %rsp	/* set up stack */
	movq	%rcx, %rax	/* set up env */
        callq    *%rdx		/* call function */

	/* exit the thread */
	movq	$0x2000169, %rax	/* Sysbsdthread_terminate */
	movq	%rsp, %rdi	/* stack */
	movq	$0, %rsi	/* len */
	movq	$0, %rdx	/* sem */
	syscall
	
