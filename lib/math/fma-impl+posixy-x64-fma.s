.globl math$fma32
.globl _math$fma32
math$fma32:
_math$fma32:
	vfmadd132ss %xmm1, %xmm2, %xmm0
	ret

.globl math$fma64
.globl _math$fma64
math$fma64:
_math$fma64:
	vfmadd132sd %xmm1, %xmm2, %xmm0
	ret
