/*
const thread.exit	: (stacksz : std.size -> void)
NOTE: must be called from the bottom of the stack, since
we assume that %rbp is in the top 4k of the stack.
*/
.globl thread$exit
thread$exit:
	/* find top of stack */
	movq	%rbp,%rdi	/* addr */
	andq	$~0xfff,%rdi	/* align it */
	addq	$0x1000,%rdi

	/* munmap(base, size) */
	movq	$11,%rax	/* munmap */
	movq	-8(%rdi),%rsi	/* size */
	subq	%rsi,%rdi	/* move to base ptr */
	syscall

	/* thread_exit(0) */
	movq	$60,%rax	/* exit */
	xorq	%rdi,%rdi	/* 0 */
	syscall
	
