.globl math$trunc32
.globl math$_trunc32
math$trunc32:
math$_trunc32:
	roundss $0x03, %xmm0, %xmm0
	ret

.globl math$floor32
.globl math$_floor32
math$floor32:
math$_floor32:
	roundss $0x01, %xmm0, %xmm0
	ret

.globl math$ceil32
.globl math$_ceil32
math$ceil32:
math$_ceil32:
	roundss $0x02, %xmm0, %xmm0
	ret

.globl math$trunc64
.globl math$_trunc64
math$trunc64:
math$_trunc64:
	roundsd $0x03, %xmm0, %xmm0
	ret

.globl math$floor64
.globl math$_floor64
math$floor64:
math$_floor64:
	roundsd $0x01, %xmm0, %xmm0
	ret

.globl math$ceil64
.globl math$_ceil64
math$ceil64:
math$_ceil64:
	roundsd $0x02, %xmm0, %xmm0
	ret
