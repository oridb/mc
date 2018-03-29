.globl math$sqrt32
.globl math$_sqrt32
math$sqrt32:
math$_sqrt32:
	sqrtss %xmm0, %xmm0
	ret

.globl math$sqrt64
.globl math$_sqrt64
math$sqrt64:
math$_sqrt64:
	sqrtsd %xmm0, %xmm0
	ret
