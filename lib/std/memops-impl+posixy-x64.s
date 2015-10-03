/*
std.memblit	: (dst : byte#, src : byte#, len : std.size -> void)
std.memfill	: (dst : byte#, val : byte, len : std.size -> void)
*/
.globl _std$memblit
.globl std$memblit
_std$memblit:
std$memblit:
	cmpq	%rdi,%rsi
	jz	.done
	jg	.fwdcpy
	movq	%rsi,%rax
	subq	%rdi,%rax
	cmpq	%rax,%rcx
	jg	.revcpy
.fwdcpy:
	movq	%rdx,%rcx
	shrq	$3,%rcx
	rep  movsq
	movq	%rdx,%rcx
	andq	$7,%rcx
	rep  movsb
	jmp	.done
.revcpy:
	std
	movq	%rdx,%rcx
	leaq	-1(%rdx,%rsi),%rsi
	leaq	-1(%rdx,%rdi),%rdi
	rep movsb
	cld
.done:
	ret

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
