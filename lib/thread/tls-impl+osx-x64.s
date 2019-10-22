.set tid,	0x00
.set len,	0x08
.set self,	0x20
.set slots,	0x28

/* const tid : (-> tid) */
.globl thread$tid
.globl _thread$tid
thread$tid:
_thread$tid:
	movq	%gs:tid, %rax
	ret

/* const _tlsset : (k : key, v : void# -> void) */
.globl thread$_tlsset
.globl _thread$_tlsset
thread$_tlsset:
_thread$_tlsset:
	cmpq	%gs:len, %rdi
	jnb	oob

	movq	$slots, %r10
	movq	%rsi, %gs:(%r10, %rdi, 0x8)
	ret

/* const _tlsget : (k : key -> void#) */
.globl thread$_tlsget
.globl _thread$_tlsget
thread$_tlsget:
_thread$_tlsget:
	cmpq	%gs:len, %rdi
	jnb	oob

	movq	$slots, %r10
	movq	%gs:(%r10, %rdi, 0x8), %rax
	ret

oob:
	call	_thread$tlsoob

/* const tlslen : (-> key) */
.globl thread$tlslen
.globl _thread$tlslen
thread$tlslen:
_thread$tlslen:
	movq	%gs:len, %rax
	ret

/* const _setgsbase : (h : tlshdr# -> int64) */
.globl thread$_setgsbase
.globl _thread$_setgsbase
thread$_setgsbase:
_thread$_setgsbase:
	movq	$0x3000003, %rax /* undocumented syscall; sets %gs to %rdi */
	syscall
	ret

/* const getgsbase : (-> tlshdr#) */
.globl thread$getgsbase
.globl _thread$getgsbase
thread$getgsbase:
_thread$getgsbase:
	movq	%gs:self, %rax
	ret
