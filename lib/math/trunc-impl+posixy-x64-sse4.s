.globl math$trunc32
.globl _math$trunc32
math$trunc32:
_math$trunc32:
	roundss $0x03, %xmm0, %xmm0
	ret

.globl math$floor32
.globl _math$floor32
math$floor32:
_math$floor32:
	roundss $0x01, %xmm0, %xmm0
	ret

.globl math$ceil32
.globl _math$ceil32
math$ceil32:
_math$ceil32:
	roundss $0x02, %xmm0, %xmm0
	ret

.globl math$trunc64
.globl _math$trunc64
math$trunc64:
_math$trunc64:
	roundsd $0x03, %xmm0, %xmm0
	ret

.globl math$floor64
.globl _math$floor64
math$floor64:
_math$floor64:
	roundsd $0x01, %xmm0, %xmm0
	ret

.globl math$ceil64
.globl _math$ceil64
math$ceil64:
_math$ceil64:
	roundsd $0x02, %xmm0, %xmm0
	ret
