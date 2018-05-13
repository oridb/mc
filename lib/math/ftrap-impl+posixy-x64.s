.globl _math$fptrap
.globl math$fptrap
_math$fptrap:
math$fptrap:
	subq	$4,%rsp
	wait
	stmxcsr	(%rsp)
	movl	(%rsp),%eax
	andl	$~0x1f80,%eax
	testb	%dil,%dil
	jnz	.apply
	orl	$0x1f80,%eax
.apply:
	movl	%eax,(%rsp)
	ldmxcsr	(%rsp)
	addq	$4,%rsp
	ret
