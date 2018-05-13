.globl _math$fptrap
.globl math$fptrap
_math$fptrap:
math$fptrap
	subq	$4,%rsp
	wait
	stmxcsr	(%rsp)
	movl	(%rsp),%rax
	andl	$~0x1f80,%rax
	testb	%rdi,%rdi
	jnz	.apply
	orl	$0x1f80,%rax
.apply:
	movl	%rax,(rsp)
	ldmxcsr	(%rsp)
	addq	$4,%rsp
	ret
