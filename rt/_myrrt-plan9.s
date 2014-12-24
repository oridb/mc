#define NPRIVATES	16

TEXT	_main(SB), 1, $(2*8+NPRIVATES*8)
	MOVQ	AX, _tos(SB)
	LEAQ	16(SP), AX
	MOVQ	AX, _privates(SB)
	MOVL	$NPRIVATES, _nprivates(SB)
	MOVL	inargc-8(FP), RARG
	LEAQ	inargv+0(FP), AX
	MOVQ	AX, 8(SP)
	CALL	main(SB)
exitloop:
	MOVQ	$0,estatus+0(FP)
	MOVQ	$8,RARG
	SYSCALL
	JMP		exitloop

TEXT	_rt$abort_oob(SB),1,$0
broke:
	XORQ	AX,AX
	MOVQ	$1234,(AX)
	JMP		broke

GLOBL	argv0(SB), $8
GLOBL	_tos(SB), $8
GLOBL	_privates(SB), $8
GLOBL	_nprivates(SB), $4
