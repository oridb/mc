/*
 * Allocates a C string on the stack, for
 * use within system calls, which is the only
 * place the Myrddin stack should need nul-terminated
 * strings.
 *
 * This is in assembly, because for efficiency we
 * allocate the C strings on the stack, and don't adjust
 * %rsp when returning.
 */
.globl std$cstring
.globl _std$cstring
_std$cstring:
std$cstring:
	movq (%rsp),%r15	/* ret addr */
	movq 8(%rsp),%rsi	/* src */
        movq %rsp,%rdi          /* dest */
	movq %rsp,%rax          /* ret val */
	movq 16(%rsp),%rcx	/* len */

        subq $16,%rsp           /* compensate for args */
	subq %rcx,%rsp          /* get stack */
        subq $1,%rsp            /* nul */
        andq $(~15),%rsp        /* align */

	cld
	rep movsb
        movb $0,(%rdi)          /* terminate */

	pushq %r15              /* ret addr */
	ret

