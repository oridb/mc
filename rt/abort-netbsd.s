.text

.globl _rt$abort_oob
.globl __rt$abort_oob
_rt$abort_oob:
__rt$abort_oob:
	/* format pc */
	movq	(%rsp),%rax
	movq	$15,%rdx
	leaq	.digitchars(%rip),%r8
	leaq    .pcstr(%rip),%r9
.loop:
	movq	%rax, %rcx
	andq	$0xf, %rcx
	movb    (%r8,%rcx),%r10b
	movb	%r10b,(%r9,%rdx)
	subq	$1, %rdx
	shrq	$4, %rax
	jnz .loop
	/* write abort message */
	movq	$4, %rax 	/* write(fd=%rdi, msg=%rsi, len=%rdx) */
	movq	$2, %rdi		/* fd */
	leaq	.msg(%rip), %rsi	/* msg */
	movq	$(.msgend-.msg), %rdx	/* length */
	syscall
	/* kill self */
	movq	$20,%rax 	/* getpid */
	syscall	
	movq	%rax, %rdi	/* save pid */
	movq	$37, %rax	/* kill(pid=%rdi, sig=%rsi) */
	movq	$6, %rsi
	syscall
.data
.msg: 	/* pc name:  */
	.ascii "0x"
.pcstr:
	.ascii "0000000000000000"
	.ascii ": out of bounds access\n"
.msgend:

.digitchars:
	.ascii "0123456789abcdef"
