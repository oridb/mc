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
.globl sys$cstring
.globl _sys$cstring
_sys$cstring:
sys$cstring:
	/* save registers */
	pushq %rbp
	movq %rsp,%rbp
	pushq %r15
	pushq %rsi
	pushq %rdi
	pushq %rcx

	movq 8(%rbp),%r15	/* ret addr */
	movq 16(%rbp),%rsi	/* src */
	movq 24(%rbp),%rcx	/* len */

	subq %rcx,%rsp		/* get stack */
	subq $1,%rsp		/* +1 for nul */
	movq %rsp,%rdi		/* dest */
	movq %rsp,%rax		/* ret val */
	subq $31,%rsp		/* "unpop" the args */
	andq $(~15),%rsp	/* align */

	cld
	rep movsb
	movb $0,(%rdi)		/* terminate */

	pushq %r15		/* ret addr */

	/* restore registers */
	movq -32(%rbp),%rcx
	movq -24(%rbp),%rdi
	movq -16(%rbp),%rsi
	movq -8(%rbp),%r15
	movq (%rbp),%rbp
	ret

.globl sys$alloca
.globl _sys$alloca
_sys$alloca:
sys$alloca:
	/* save registers */
	pushq %rbp
	movq %rsp,%rbp
	pushq %r15
	pushq %rbx

	movq 8(%rbp),%r15	/* ret addr */

	/* get stack space */
	subq %rdi,%rsp		/* get stack space */
	movq %rsp,%rax		/* top of stack (return value) */
	subq $31,%rsp		/* "unpop" the args for return */
	andq $(~15),%rsp	/* align */

	pushq %r15		/* ret addr */

	/* restore registers */
	movq -16(%rbp),%rbx
	movq -8(%rbp),%r15
	movq (%rbp),%rbp
	ret
