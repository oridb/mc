.globl _std$syscall
_std$syscall:
        popq %rcx	/* return address */
	popq %rax       /* call num */
	pushq %rcx
        syscall
	pushq %rcx	/* put the return address back */
	ret
