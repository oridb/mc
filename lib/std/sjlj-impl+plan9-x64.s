TEXT std$setjmp+0(SB),$0
	/* save registers */
	MOVQ	BX,   (DI)
	MOVQ	BP,  8(DI)
	MOVQ	R12, 16(DI)
	MOVQ	R13, 24(DI)
	MOVQ	R14, 32(DI)
	MOVQ	R15, 40(DI)

	/* save sp */
	LEAQ	8(SP),DX
	MOVQ	DX, 48(DI)

	/* save ip */
	MOVQ	(SP), DX
	MOVQ	DX, 56(DI)
	XORQ	AX, AX
	RET

TEXT std$longjmp+0(SB),$0
	/* return true */
	MOVQ	$1, AX

	/* restore registers */
	MOVQ	  (DI), BX
	MOVQ	 8(DI), BP
	MOVQ	16(DI), R12
	MOVQ	24(DI), R13
	MOVQ	32(DI), R14
	MOVQ	40(DI), R15
	MOVQ	48(DI), DX

	/* load stack */
	MOVQ	DX, SP

	/* jmp to return addr */
	MOVQ	56(DI), DX
	JMP	*DX
