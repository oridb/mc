.globl thread$xget32
thread$xget32:
	movl	(%rdi), %eax
	ret
.globl thread$xget64
thread$xget64:
	movq	(%rdi), %rax
	ret
.globl thread$xset32
thread$xset32:
	movl	%esi, (%rdi)
	ret
.globl thread$xset64
thread$xset64:
	movq	%rsi, (%rdi)
	ret
.globl thread$xadd32
thread$xadd32:
	lock xaddl	%esi, (%rdi)
	ret
.globl thread$xadd64
thread$xadd64:
	lock xaddq	%rsi, (%rdi)
	ret
.globl thread$xsub32
thread$xsub32:
	lock xaddl	%esi, (%rdi)
	ret
.globl thread$xsub64
thread$xsub64:
	lock xaddq	%rsi, (%rdi)
	ret
.globl thread$xcas32
thread$xcas32:
	movl	%esi, %eax
	lock cmpxchgl	%edx, (%rdi)
	ret
.globl thread$xcas64
thread$xcas64:
	movq	%rsi, %rax
	lock cmpxchgq	%rdx, (%rdi)
	ret
.globl thread$xchg32
thread$xchg32:
	movl	%esi, %eax
	lock xchgl	(%rdi), %eax
	ret
.globl thread$xchg64
thread$xchg64:
	movq	%rsi, %rax
	lock xchgq	(%rdi), %rax
	ret
