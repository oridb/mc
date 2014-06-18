.globl std$syscall
std$syscall:
	pushq %rbp 
	pushq %rdi 
	pushq %rsi 
	pushq %rdx 
	pushq %r10 
	pushq %r8
	pushq %r9
	pushq %rcx 
	pushq %r11 
	/*
	hack: We load 6 args regardless of
	how many we actually have. This may
	load junk values, but if the syscall
	doesn't use them, it's going to be
	harmless.
	 */
	movq 80 (%rsp),%rax
	movq 88 (%rsp),%rdi
	movq 96 (%rsp),%rsi
	movq 104(%rsp),%rdx
	movq 112(%rsp),%r10
	movq 120(%rsp),%r8
	movq 128(%rsp),%r9

	syscall

	popq %r11
	popq %rcx
	popq %r9
	popq %r8
	popq %r10
	popq %rdx
	popq %rsi
	popq %rdi
	popq %rbp
	ret

