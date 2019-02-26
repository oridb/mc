#define NPRIVATES	16
/*
 * counts the length of the string pointed to
 * by %r8, returning len in %r9. Does not modify
 * any registers outside of %r9
 */
TEXT cstrlen(SB),$0
	XORQ	R9,R9
	JMP	.lentest
.lenloop:
	INCQ	R9
.lentest:
	CMPB	0(R8)(R9*1),$0
	JNE	.lenloop
	RET

/*
 * iterate over the strings for argc, and put
 * them into the args array.
 * 
 * argc in %rax, argv in %rbx, dest vector in %rcx
 */
TEXT cvt(SB),$0
	JMP	.cvttest
.cvtloop:
	SUBQ	$1,AX
	MOVQ	(BX),R8
	CALL	cstrlen(SB)
	MOVQ	R8,(CX)
	MOVQ	R9,8(CX)
	ADDQ	$8,BX
	ADDQ	$16,CX
.cvttest:
	TESTQ	AX,AX
	JNE	.cvtloop
.cvtdone:
	RET
	

TEXT	_main(SB), 1, $(72+NPRIVATES*8)
	MOVQ	AX, sys$tosptr(SB)
	MOVL	inargc-8(FP), R13
	LEAQ	inargv+0(FP), R14
	MOVQ	R13, AX
	IMULQ	$16,AX
	SUBQ	AX,SP
	MOVQ	SP,DX

	MOVQ	R13, AX
	MOVQ	R14, BX
	MOVQ	SP, CX
	CALL	cvt(SB)
	PUSHQ	R13
	PUSHQ	DX

	XORQ	BP,BP
	CALL	__init__(SB)
	CALL	main(SB)
	POPQ	DX
	POPQ	R13
	CALL	__fini__(SB)

exitloop:
	MOVQ	$0,8(SP)
	MOVQ	$8,RARG
	SYSCALL
	JMP		exitloop

TEXT	_rt$abort_oob(SB),1,$0
broke:
	XORQ	AX,AX
	MOVQ	$1234,(AX)
	JMP		broke

GLOBL	argv0(SB), $8
GLOBL	sys$tosptr(SB), $8
