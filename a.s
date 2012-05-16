.globl _main
_main:
	pushl %ebp
	movl %esp,%ebp
	subl $12,%esp
	jmp .L2
.L1:
	movl 4(%esp),%eax
	addl (%esp),%eax
	movl %eax,4(%esp)
.L2:
	movl (%esp),%eax
	cmpl $10,%eax
	jl .L1
	jmp .L3
.L3:
	movl 4(%esp),%eax
	movl %eax,(%esp)
	jmp .L0
.L0:
	movl (%esp),%eax
	movl %ebp,%esp
	popl %ebp
	ret
