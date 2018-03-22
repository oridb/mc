.globl math$fma32
.globl math$_fma32
math$fma32:
math$_fma32:
	vfmadd132ss %xmm1, %xmm2, %xmm0
	ret

.globl math$fma64
.globl math$_fma64
math$fma64:
math$_fma64:
	vfmadd132sd %xmm1, %xmm2, %xmm0
	ret
