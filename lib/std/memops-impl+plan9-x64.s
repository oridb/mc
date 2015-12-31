/*
std.memblit	: (dst : byte#, src : byte#, len : std.size -> void)
std.memfill	: (dst : byte#, val : byte, len : std.size -> void)
*/
TEXT std$memblit+0(SB),$0
	CMPQ	SI,DI
	JZ	.done
	JG	.fwdcpy
	MOVQ	SI,AX
	SUBQ	DI,AX
	CMPQ	CX,AX
	JG	.revcpy
.fwdcpy:
	MOVQ	DX,CX
	SHRQ	$3,CX
	REP; MOVSQ
	MOVQ	DX,CX
	ANDQ	$7,CX
	REP; MOVSB
	JMP	.done
.revcpy:
	STD
	MOVQ	DX,CX
	LEAQ	-1(DX)(SI*1),SI
	LEAQ	-1(DX)(DI*1),DI
	REP; MOVSB
	CLD
.done:
	RET

TEXT std$memfill+0(SB),$0
	/* generate 8 bytes of fill */
	MOVBQZX	SI,BX
	MOVQ	$0x101010101010101,AX
	IMULQ	BX,AX

	/* and fill */
	MOVQ	DX,CX
	SHRQ	$3,CX
	REP; STOSQ
	MOVQ	DX,CX
	ANDQ	$7,CX
	REP; STOSB 
	RET
