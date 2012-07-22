.globl _std$syscall
_std$syscall:
        popl %ecx	/* return address */
	popl %eax       /* call num */
	pushl %ecx
        int $0x80
	pushl %ecx	/* put the return address back */
	ret
