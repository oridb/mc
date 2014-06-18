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
	movq 16(%rsp),%rcx	/* len */
	
	subq %rcx,%rsp          /* get stack */
	movq %rsp,%rdi          /* dest */
	movq %rsp,%rax          /* ret val */
	subq $16,%rsp		/* "unpop" the args */
	subq $1,%rsp            /* nul */
	andq $(~15),%rsp        /* align */
	
	cld
	rep movsb
	movb $0,(%rdi)          /* terminate */
	
	pushq %r15              /* ret addr */
	ret

.globl std$alloca
.globl _std$alloca
_std$alloca:
std$alloca:
	movq (%rsp),%r15	/* ret addr */
	movq 8(%rsp),%rbx	/* len */
	
	/* get stack space */
	subq %rbx,%rsp          /* get stack space */
	movq %rsp,%rax          /* top of stack (return value) */
	subq $16,%rsp		/* "unpop" the args for return */
	andq $(~15),%rsp        /* align */

	pushq %r15              /* ret addr */
	ret
