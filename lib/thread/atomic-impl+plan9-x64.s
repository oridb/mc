TEXT thread$xget32+0(SB),1,$0
	MOVL	(DI), AX
	RET
TEXT thread$xget64+0(SB),1,$0
	MOVQ	(DI), AX
	RET
TEXT thread$xgetp+0(SB),1,$0
	MOVQ	(DI), AX
	RET

TEXT thread$xset32+0(SB),1,$0
	MOVL	SI, (DI)
	RET
TEXT thread$xset64+0(SB),1,$0
	MOVQ	SI, (DI)
	RET
TEXT thread$xsetp+0(SB),1,$0
	MOVQ	SI, (DI)
	RET

TEXT thread$xadd32+0(SB),1,$0
	LOCK; XADDL	SI, (DI)
	MOVL	SI, AX
	RET
TEXT thread$xadd64+0(SB),1,$0
	LOCK; XADDQ	SI, (DI)
	MOVQ	SI, AX
	RET
TEXT thread$xaddp+0(SB),1,$0
	LOCK; XADDQ	SI, (DI)
	MOVQ	SI, AX
	RET

TEXT thread$xsub32+0(SB),1,$0
	LOCK; XADDL	SI, (DI)
	RET
TEXT thread$xsub64+0(SB),1,$0
	LOCK; XADDQ	SI, (DI)
	RET
TEXT thread$xsubp+0(SB),1,$0
	LOCK; XADDQ	SI, (DI)
	RET

TEXT thread$xcas32+0(SB),1,$0
	MOVL	SI, AX
	LOCK; CMPXCHGL	DX, (DI)
	RET
TEXT thread$xcas64+0(SB),1,$0
	MOVQ	SI, AX
	LOCK; CMPXCHGQ	DX, (DI)
	RET
TEXT thread$xcasp+0(SB),1,$0
	MOVQ	SI, AX
	LOCK; CMPXCHGQ	DX, (DI)
	RET

TEXT thread$xchg32+0(SB),1,$0
	MOVL	SI, AX
	LOCK; XCHGL	(DI), AX
	RET
TEXT thread$xchg64+0(SB),1,$0
	MOVQ	SI, AX
	LOCK; XCHGQ	(DI), AX
	RET
TEXT thread$xchgp+0(SB),1,$0
	MOVQ	SI, AX
	LOCK; XCHGQ	(DI), AX
	RET
