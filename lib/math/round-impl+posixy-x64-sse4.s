.globl math$rn32
.globl math$_rn32
math$rn32:
math$_rn32:
	cvtss2si	%xmm0, %eax
	ret

.globl math$rn64
.globl math$_rn64
math$rn64:
math$_rn64:
	cvtsd2si	%xmm0, %rax
	ret

