.globl start
start:
    /* turn args into a slice */
    /* TODO */
    /* enter the main program */
    call	_main
    /* exit */
    movq	%rax,%rdi
    movq	$60,%rax
    syscall

