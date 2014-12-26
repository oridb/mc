/*
 * Allocates a C string on the stack, for
 * use within system calls, which is the only
 * place the Myrddin stack should need nul-terminated
 * strings.
 *
 * This is in assembly, because for efficiency we
 * allocate the C strings on the stack, and don't adjust
 * SP when returning.
 */
TEXT sys$cstring+0(SB),$0
	/* save registers */
	MOVQ	SP,BP
	SUBQ	$48,SP
	MOVQ	BP,-8(BP)
	MOVQ	R15,-16(BP)
	MOVQ	SI,-24(BP)
	MOVQ	DI,-32(BP)
	MOVQ	CX,-40(BP)

	MOVQ 	(BP),R15	/* ret addr */
	MOVQ	8(BP),SI	/* src */
	MOVQ	16(BP),CX	/* len */

	SUBQ	CX,SP		/* get stack */
	SUBQ	$1,SP		/* +1 for nul */
	MOVQ	SP,DI		/* dest */
	MOVQ	SP,AX		/* ret val */
	SUBQ	$16,SP		/* "unpop" the args */
	ANDQ	$(~15),SP	/* align */

	CLD
	REP
        MOVSB
	MOVB	$0,(DI)		/* terminate */

	/* Restore registers */
	MOVQ	R15,0(SP)	/* place ret addr */
	MOVQ	-40(BP),CX
	MOVQ	-32(BP),DI
	MOVQ	-24(BP),SI
	MOVQ	-16(BP),R15
	MOVQ	-8(BP) ,BP
	RET

TEXT sys$alloca+0(SB),$0
	/* save registers */
        MOVQ    SP,BP
	SUBQ	$32,SP
	MOVQ	BP,-8(BP)
	MOVQ	R15,-16(BP)
	MOVQ	BX,-24(SP)

	MOVQ	(BP),R15	/* ret addr */
	MOVQ	8(BP),BX	/* len */

	/* get stack space */
	SUBQ	BX,SP		/* get stack space */
	MOVQ	SP,AX		/* top of stack (return value) */
	SUBQ	$16,SP		/* "unpop" the args for return */
	ANDQ	$(~15),SP	/* align */

	MOVQ	R15,0(SP)	/* place ret addr */
	/* restore registers */
	MOVQ	-16(BP),R15
	MOVQ	-24(SP),BX
	MOVQ	-8(BP),BP
	RET

GLOBL	sys$tosptr+0(SB),$8
DATA	sys$tosptr+0(SB)/8,$_tos+0(SB)
GLOBL	sys$curbrk+0(SB),$8
DATA	sys$curbrk+0(SB)/8,$end+0(SB)

