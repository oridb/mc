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
	PUSHQ   BP
	MOVQ	SP,BP
	PUSHQ	R15
	PUSHQ	SI
	PUSHQ	DI
	PUSHQ	CX

	MOVQ 	8(BP),R15	/* ret addr */
	MOVQ	16(BP),SI	/* src */
	MOVQ	24(BP),CX	/* len */

	SUBQ	CX,SP		/* get stack */
	SUBQ	$1,SP		/* +1 for nul */
	MOVQ	SP,DI		/* dest */
	MOVQ	SP,AX		/* ret val */
	SUBQ	$16,SP		/* "unpop" the args */
	ANDQ	$(~15),SP	/* align */

	CLD
	REP; MOVSB
	MOVB    $0,(DI)		/* terminate */

	PUSHQ R15		/* ret addr */

	/* Restore registers */
	MOVQ	-32(BP),CX
	MOVQ	-24(BP),DI
	MOVQ	-16(BP),SI
	MOVQ	-8(BP),R15
	MOVQ	(BP),BP
	RET

TEXT sys$alloca+0(SB),$0
	/* save registers */
	PUSHQ   BP
	MOVQ    SP,BP
	PUSHQ   R15
	PUSHQ   BX

	MOVQ	8(BP),R15	/* ret addr */
	MOVQ	16(BP),BX	/* len */

	/* get stack space */
	SUBQ    BX,SP		/* get stack space */
	MOVQ    SP,AX		/* top of stack (return value) */
	SUBQ    $16,SP		/* "unpop" the args for return */
	ANDQ    $(~15),SP	/* align */

	PUSHQ R15		/* ret addr */

	/* restore registers */
	MOVQ    -16(BP),BX
	MOVQ    -8(BP),R15
	MOVQ    (BP),BP
	RET
