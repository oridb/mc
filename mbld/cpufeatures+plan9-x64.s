TEXT bld$cpufeatures+0(SB),$0
	MOVL	$0x1,AX
	CPUID
	MOVL    CX, AX
	MOVL    DX, DX
	ROLQ    $32, DX
	ORQ     DX, AX
	RET
