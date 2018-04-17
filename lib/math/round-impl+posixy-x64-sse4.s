.globl math$rn32
.globl math$_rn32
math$rn32:
math$_rn32:
	pushq	%rbp
	movq	%rsp, %rbp
	subq	$16, %rsp
	fnstcw	(%rsp)
	fnstcw	8(%rsp)
	andl	$0xf3ff, 8(%rsp)
	fldcw	8(%rsp)
	cvtss2si	%xmm0, %eax
	fldcw	(%rsp)
	leave
	ret

.globl math$rn64
.globl math$_rn64
math$rn64:
math$_rn64:
	pushq	%rbp
	movq	%rsp, %rbp
	subq	$16, %rsp
	fnstcw	(%rsp)
	fnstcw	8(%rsp)
	andl	$0xf3ff, 8(%rsp)
	fldcw	8(%rsp)
	cvtsd2si	%xmm0, %rax
	fldcw	(%rsp)
	leave
	ret

