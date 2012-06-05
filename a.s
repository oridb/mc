.globl main
main:
	pushl %ebp
	movl %esp,%ebp
	subl $12,%esp
	leal (%esp),%eax
	addl $0,%eax
	movl $12,(%eax)
	leal (%esp),%eax
	addl $4,%eax
	movl $30,(%eax)
	leal (%esp),%eax
	addl $0,%eax
	movl %eax,%ecx
	leal (%esp),%edx
	addl $4,%edx
	movl %edx,%ebx
	addl %ebx,%ecx
	movl %ecx,8(%esp)
	jmp .L0
.L0:
	movl 8(%esp),%eax
	movl %ebp,%esp
	popl %ebp
	ret
