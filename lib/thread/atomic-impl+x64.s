# get variants
.globl thread$xget8
.globl _thread$xget8
thread$xget8:
_thread$xget8:
	movb	(%rdi), %al
	ret
.globl thread$xget32
.globl _thread$xget32
thread$xget32:
_thread$xget32:
	movl	(%rdi), %eax
	ret
.globl thread$xget64
.globl thread$xgetp
.globl _thread$xget64
.globl _thread$xgetp
thread$xget64:
thread$xgetp:
_thread$xget64:
_thread$xgetp:
	movq	(%rdi), %rax
	ret

# set variants
.globl thread$xset8
.globl _thread$xset8
thread$xset8:
_thread$xset8:
	lock xchgb	(%rdi), %sil
	ret
.globl thread$xset32
.globl _thread$xset32
thread$xset32:
_thread$xset32:
	lock xchgl	(%rdi), %esi
	ret
.globl thread$xset64
.globl thread$xsetp
.globl _thread$xset64
.globl _thread$xsetp
thread$xset64:
thread$xsetp:
_thread$xset64:
_thread$xsetp:
	lock xchgq	(%rdi), %rsi
	ret

# add variants
.globl thread$xadd8
.globl _thread$xadd8
thread$xadd8:
_thread$xadd8:
	lock xaddb	%sil, (%rdi)
	movb %sil,%al
	ret
.globl thread$xadd32
.globl _thread$xadd32
thread$xadd32:
_thread$xadd32:
	lock xaddl	%esi, (%rdi)
	movl %esi,%eax
	ret
.globl thread$xadd64
.globl thread$xaddp
.globl _thread$xadd64
.globl _thread$xaddp
thread$xadd64:
thread$xaddp:
_thread$xadd64:
_thread$xaddp:
	lock xaddq	%rsi, (%rdi)
	movq %rsi,%rax
	ret

# cas variants 
.globl thread$xcas8
.globl _thread$xcas8
thread$xcas8:
_thread$xcas8:
	movb	%sil, %al
	lock cmpxchgb	%dl, (%rdi)
	ret
.globl thread$xcas32
.globl _thread$xcas32
thread$xcas32:
_thread$xcas32:
	movl	%esi, %eax
	lock cmpxchgl	%edx, (%rdi)
	ret
.globl thread$xcas64
.globl thread$xcasp
.globl _thread$xcas64
.globl _thread$xcasp
thread$xcas64:
thread$xcasp:
_thread$xcas64:
_thread$xcasp:
	movq		%rsi, %rax
	lock cmpxchgq	%rdx, (%rdi)
	ret

# xchg variants
.globl thread$xchg8
.globl _thread$xchg8
thread$xchg8:
_thread$xchg8:
	movb		%sil, %al
	lock xchgb	(%rdi), %al
	ret
.globl thread$xchg32
.globl _thread$xchg32
thread$xchg32:
_thread$xchg32:
	movl	%esi, %eax
	lock xchgl	(%rdi), %eax
	ret
.globl thread$xchg64
.globl thread$xchgp
.globl _thread$xchg64
.globl _thread$xchgp
thread$xchg64:
thread$xchgp:
_thread$xchg64:
_thread$xchgp:
	movq	%rsi, %rax
	lock xchgq	(%rdi), %rax
	ret
