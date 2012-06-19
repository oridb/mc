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
.a5:	movl 32(%ebp),%edi
.a4:	movl 28(%ebp),%esi
.a3:	movl 24(%ebp),%edx
.a2:	movl 20(%ebp),%ecx
.a1:	movl 16(%ebp),%ebx
	/* 12(%ebp) holds nargs */
.a0:	movl 8(%ebp),%eax
        int $0x80
	movl %ebp,%esp
	popl %ebp
	ret
