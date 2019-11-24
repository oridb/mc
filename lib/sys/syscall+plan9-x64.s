/*
Ugly: Kernel is caller-save, Myrddin
is callee-save. We need to preserve
registers before entering the kernel.

However, we also need SP to point to the
start of the arguments.

Luckily, the kernel doesn't touch our stack,
and we have 256 bytes of gap if we get a note.
*/
TEXT sys$syscall+0(SB),1,$0
	MOVQ	R11,-16(SP)
	MOVQ	R12,-24(SP)
	MOVQ	R13,-32(SP)
	MOVQ	R14,-40(SP)
	MOVQ	R15,-48(SP)
	MOVQ	BP,-56(SP)
	MOVQ	8(SP),RARG

	ADDQ	$8,SP
	MOVQ	DI, RARG
	SYSCALL
	SUBQ	$8,SP

	MOVQ	-16(SP),R11
	MOVQ	-24(SP),R12
	MOVQ	-32(SP),R13
	MOVQ	-40(SP),R14
	MOVQ	-48(SP),R15
	MOVQ	-56(SP),BP
	RET

