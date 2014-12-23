.globl sys$syscall
sys$syscall:
	/*
        Ugly: Kernel is caller-save, Myrddin
        is callee-save. We need to preserve
        registers before entering the kernel.

        However, we also need SP to point to the
        start of the arguments.

	Luckily, the kernel doesn't touch our stack,
	and we have 256 bytes of gap if we get a note.
        */
        MOVQ BX,-16(SP)
        MOVQ CX,-24(SP)
        MOVQ DX,-32(SP)
        MOVQ SI,-40(SP)
        MOVQ DI,-48(SP)
        MOVQ BP,-56(SP)
        MOVQ R8,-64(SP)
        MOVQ R9,-72(SP)
        MOVQ R10,-80(SP)
        MOVQ R11,-88(SP)
        MOVQ R12,-104(SP)
        MOVQ R13,-112(SP)
        MOVQ R14,-120(SP)
        MOVQ R15,-128(SP)
        ADDQ $8,SP

	SYSCALL

	SUBQ $8,SP
        MOVQ -16(SP),BX
        MOVQ -24(SP),CX
        MOVQ -32(SP),DX
        MOVQ -40(SP),SI
        MOVQ -48(SP),DI
        MOVQ -56(SP),BP
        MOVQ -64(SP),R8
        MOVQ -72(SP),R9
        MOVQ -80(SP),R10
        MOVQ -88(SP),R11
        MOVQ -104(SP),R12
        MOVQ -112(SP),R13
        MOVQ -120(SP),R14
        MOVQ -128(SP),R15
	RET

