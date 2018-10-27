.set tid,	0x00
.set len,	0x04
.set slots,	0x18

/* const tid : (-> tid) */
.globl thread$tid
.globl _thread$tid
thread$tid:
_thread$tid:
	movl	%fs:tid, %eax
	ret

/* const _tlsset : (k : key, v : void# -> void) */
.globl thread$_tlsset
.globl _thread$_tlsset
thread$_tlsset:
_thread$_tlsset:
	cmpl	%fs:len, %edi
	jnb	oob

	movslq	%edi, %rdi
	movq	$slots, %r10
	movq	%rsi, %fs:(%r10, %rdi, 0x8)
	ret

/* const _tlsget : (k : key -> void#) */
.globl thread$_tlsget
.globl _thread$_tlsget
thread$_tlsget:
_thread$_tlsget:
	cmpl	%fs:len, %edi
	jnb	oob

	movslq	%edi, %rdi
	movq	$slots, %r10
	movq	%fs:(%r10, %rdi, 0x8), %rax
	ret

oob:
	call	thread$tlsoob

/* const tlslen : (-> key) */
.globl thread$tlslen
.globl _thread$tlslen
thread$tlslen:
_thread$tlslen:
	movl	%fs:len, %eax
	ret
