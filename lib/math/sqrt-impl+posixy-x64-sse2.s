.globl math$sqrt32
.globl _math$sqrt32
math$sqrt32:
_math$sqrt32:
	sqrtss %xmm0, %xmm0
	ret

.globl math$sqrt64
.globl _math$sqrt64
math$sqrt64:
_math$sqrt64:
	sqrtsd %xmm0, %xmm0
	ret
