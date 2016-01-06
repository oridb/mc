.globl thread$xget32
thread$xget32:
	movl	(%rdi), %eax
	ret
.globl thread$xget64
.globl thread$xgetp
thread$xget64:
thread$xgetp:
	movq	(%rdi), %rax
	ret

.globl thread$xset32
thread$xset32:
	movl	%esi, (%rdi)
	ret
.globl thread$xset64
.globl thread$xsetp
thread$xset64:
thread$xsetp:
	movq	%rsi, (%rdi)
	ret

.globl thread$xadd32
thread$xadd32:
	lock xaddl	%esi, (%rdi)
	movl %esi,%eax
	ret
.globl thread$xadd64
.globl thread$xaddp
thread$xadd64:
thread$xaddp:
	lock xaddq	%rsi, (%rdi)
	movq %rsi,%rax
	ret

.globl thread$xcas32
thread$xcas32:
	movl	%esi, %eax
	lock cmpxchgl	%edx, (%rdi)
	ret
.globl thread$xcas64
.globl thread$xcasp
thread$xcas64:
thread$xcasp:
	movq	%rsi, %rax
	lock cmpxchgq	%rdx, (%rdi)
	ret

.globl thread$xchg32
thread$xchg32:
	movl	%esi, %eax
	lock xchgl	(%rdi), %eax
	ret
.globl thread$xchg64
.globl thread$xchgp
thread$xchg64:
thread$xchgp:
	movq	%rsi, %rax
	lock xchgq	(%rdi), %rax
	ret
