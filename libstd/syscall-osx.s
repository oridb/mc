.globl _std$syscall
_std$syscall:
	pushq %rbp
	/*
	hack: We load 6 args regardless of
	how many we actually have. This may
	load junk values, but if the syscall
	doesn't use them, it's going to be
	harmless.
	 */
	movq 16(%rsp),%rax
	movq 24(%rsp),%rdi
	movq 32(%rsp),%rsi
	movq 40(%rsp),%rdx
	movq 48(%rsp),%r10
	movq 56(%rsp),%r8
	movq 64(%rsp),%r9

	syscall
	jae success
	negq %rax

success:
	popq %rbp
	ret
