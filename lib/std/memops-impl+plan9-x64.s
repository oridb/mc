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

TEXT std$memeq+0(SB),$0
	MOVQ	DX,R8
	ANDQ	$~0x7,R8
	JZ	.dotail
.nextquad:
	MOVQ	(DI),R9
	MOVQ	(SI),R10
	XORQ	R10,R9
	JNZ .unequal
	ADDQ	$8,SI
	ADDQ	$8,DI
	SUBQ	$8,R8
	JNZ .nextquad
.dotail:
	ANDQ	$0x7,DX
	TESTQ	DX,DX
	JZ .equal
.nextbyte:
	MOVBLZX	(DI),R9
	MOVBLZX	(SI),R10
	XORL	R10,R9
	JNZ .unequal
	ADDQ	$1,SI
	ADDQ	$1,DI
	SUBQ	$1,DX
	JNZ .nextbyte
.equal:
	MOVQ	$1,AX
	RET
.unequal:
	MOVQ	$0,AX
	RET
