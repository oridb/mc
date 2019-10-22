/* std.memblit	: (dst : byte#, src : byte#, len : std.size -> void) */
.globl _std$memblit
.globl std$memblit
_std$memblit:
std$memblit:
	cmpq	%rdi,%rsi
	movq	%rdx,%rcx
	jz	.done
	jg	.fwdcpy
	movq	%rsi,%rax
	subq	%rdi,%rax
	cmpq	%rax,%rcx
	jg	.revcpy
.fwdcpy:
	shrq	$3,%rcx
	rep  movsq
	movq	%rdx,%rcx
	andq	$7,%rcx
	rep  movsb
	jmp	.done
.revcpy:
	std
	leaq	-1(%rdx,%rsi),%rsi
	leaq	-1(%rdx,%rdi),%rdi
	rep movsb
	cld
.done:
	ret

/* std.memfill	: (dst : byte#, val : byte, len : std.size -> void) */
.globl _std$memfill
.globl std$memfill
_std$memfill:
std$memfill:
	/* generate 8 bytes of fill */
	movzbq	%sil,%rbx
	mov	$0x101010101010101,%rax
	imul	%rbx,%rax

	/* and fill */
	movq	%rdx,%rcx
	shrq	$3,%rcx
	rep stosq
	movq	%rdx,%rcx
	andq	$7,%rcx
	rep stosb 
	ret

/* std.memeq	: (a : byte#, b : byte#, len : std.size -> bool) */
.globl _std$memeq
.globl std$memeq
_std$memeq:
std$memeq:
	movq	%rdx,%r8
	andq	$~0x7,%r8
	jz	.dotail
.nextquad:
	movq	(%rdi),%r9
	movq	(%rsi),%r10
	xorq	%r10,%r9
	jnz .unequal
	addq	$8,%rsi
	addq	$8,%rdi
	subq	$8,%r8
	jnz .nextquad
.dotail:
	andq	$0x7,%rdx
	testq	%rdx,%rdx
	jz .equal
.nextbyte:
	movzbl	(%rdi),%r9d
	movzbl	(%rsi),%r10d
	xorl	%r10d,%r9d
	jnz .unequal
	addq	$1,%rsi
	addq	$1,%rdi
	subq	$1,%rdx
	jnz .nextbyte
.equal:
	movq	$1,%rax
	ret
.unequal:
	movq	$0,%rax
	ret
