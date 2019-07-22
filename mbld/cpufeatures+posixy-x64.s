.globl bld$cpufeatures
.globl _bld$cpufeatures
bld$cpufeatures:
_bld$cpufeatures:
	mov	$0x1, %eax
	cpuid
	mov	%ecx, %eax
	rol	$32, %rax
	shrd	$32, %rdx, %rax
	ret
