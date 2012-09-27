.globl _start
_start:
    /* turn args into a slice */
    /* TODO */
    /* enter the main program */
    call	main
    /* exit */
    movq	%rax,%rdi
    movq	$60,%rax
    syscall

