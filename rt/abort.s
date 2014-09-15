.text

.globl _rt$abort_oob
.globl __rt$abort_oob
_rt$abort_oob:
__rt$abort_oob:
	/* format pc */
	movq	(%rsp),%rax
	movq	$15,%rdx
.loop:
	movq	%rax, %rcx
	andq	$0xf, %rcx
	movb	.digitchars(%rcx),%r8b
	movb	%r8b,.pcstr(%rdx)
	subq	$1, %rdx
	shrq	$4, %rax
	jnz .loop
	/* write abort message */
	movq	$1, %rax 	/* write(fd=%rdi, msg=%rsi, len=%rdx) */
	movq	$2, %rdi		/* fd */
	movq	$.msg, %rsi	/* msg */
	movq	$(.msgend-.msg), %rdx	/* length */
	syscall
	/* kill self */
	movq	$39,%rax 	/* getpid */
	syscall	
	movq	%rax, %rdi	/* save pid */
	movq	$62, %rax	/* kill(pid=%rdi, sig=%rsi) */
	movq	$6, %rsi	/* kill(pid=%rdi, sig=%rsi) */
	syscall
.data
.msg: 	/* pc name:  */
	.byte '0','x'
.pcstr:
	.byte '0','0','0','0','0','0','0','0'
	.byte '0','0','0','0','0','0','0','0'
	.ascii ": out of bounds access\n"
.msgend:

.digitchars:
	.ascii "0123456789abcdef"
