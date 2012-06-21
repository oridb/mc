.globl sys$syscall
sys$syscall:
	pushl %ebp
	movl %esp,%ebp
	movl 12(%ebp),%eax #count
        shl  $2,%eax
	jmp  *.jmptab(%eax)
.jmptab:
	.long .a0
	.long .a1
	.long .a2
	.long .a3
	.long .a4
	.long .a5
	.long .a6
	/*
	   hack: 6 args uses %ebp, so we index
	   relative to %esp. This means that if
	   we actually start using stack space,
	   this code will need adjustment
	 */
.a6:	movl 36(%esp),%ebp
.a5:	movl 32(%esp),%edi
.a4:	movl 28(%esp),%esi
.a3:	movl 24(%esp),%edx
.a2:	movl 20(%esp),%ecx
.a1:	movl 16(%esp),%ebx
	/* 12(%esp) holds nargs */
.a0:	movl 8(%esp),%eax
        int $0x80
	movl %ebp,%esp
	popl %ebp
	ret
