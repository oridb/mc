.globl _sys$syscall
_sys$syscall:

	/*
	hack: We load 6 args regardless of
	how many we actually have. This may
	load junk values, but if the syscall
	doesn't use them, it's going to be
	harmless.
	 */
	movq %rdi,%rax
        /* 8(%rsp): hidden type arg */
	movq 16(%rsp),%rdi
	movq 24(%rsp),%rsi
	movq 32(%rsp),%rdx
	movq 40(%rsp),%r10
	movq 48(%rsp),%r8
	movq 56(%rsp),%r9

	syscall
	jae .success
	negq %rax

.success:
	ret

/*
 * OSX is strange about fork, and needs an assembly wrapper.
 * The fork() syscall, when called directly, returns the pid in both
 * processes, which means that both parent and child think they're
 * the parent.
 *
 * checking this involves peeking in %edx, so we need to do this in asm.
 */
.globl _sys$__osx_fork
_sys$__osx_fork:
	movq $0x2000002,%rax
	syscall

	jae .forksuccess
	negq %rax

.forksuccess:
	testl %edx,%edx
	jz .isparent
	xorq %rax,%rax
.isparent:
	ret

/*
 * OSX is strange about pipe, and needs an assembly wrapper.
 * The pipe() syscall returns the pipes created in eax:edx, and
 * needs to copy them to the destination locations manually.
 */
.globl _sys$__osx_pipe
_sys$__osx_pipe:
	movq $0x200002a,%rax
	syscall

	jae .pipesuccess
	negq %rax

.pipesuccess:
	movl %eax,(%rdi)
	movl %edx,4(%rdi)
	xorq %rax,%rax
	ret

.globl _sys$__osx_lseek
_sys$__osx_lseek:
	movq $0x20000C7,%rax
	syscall

	jae .lseeksuccess
	negq %rax

.lseeksuccess:
        shlq $32,%rdx
        orq  %rdx,%rax
	ret


.globl _sys$__osx_gettimeofday
_sys$__osx_gettimeofday:
	movq $0x2000074,%rax
	syscall

	jae .gettimeofdaysuccess
	negq %rax
	ret

.gettimeofdaysuccess:
	cmpq $0,%rax
	je .noreg

	movq %rax,0(%rdi)
	movl %edx,8(%rdi)
	xorq %rax,%rax
.noreg:
	ret

