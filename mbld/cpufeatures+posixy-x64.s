.globl bld$cpufeatures
.globl bld$_cpufeatures
bld$cpufeatures:
bld$_cpufeatures:
	mov	$0x1, %eax
	cpuid
	mov	%ecx, %eax
	rol	$32, %rax
	shrd	$32, %rdx, %rax
	ret
