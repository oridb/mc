/*
const thread.exit	: (stacksz : std.size -> void)
*/
.globl thread$exit
thread$exit:
	/* 
	  Because OpenBSD wants a valid stack whenever
	  we enter the kernel, we need to toss a preallocated
	  stack pointer into %rsp.
	 */
	movq	thread$exitstk,%rsp

	/* munmap(base, size) */
	movq	$73,%rax	/* munmap */
	movq	%fs:0x08,%rdi	/* base */
	movq	%fs:0x10,%rsi	/* stksz */
	syscall

	/* __threxit(0) */
	movq	$302,%rax	/* exit */
	xorq	%rdi,%rdi	/* 0 */
	syscall
	
