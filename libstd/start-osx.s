.globl start
start:
    /* turn args into a slice */
    /* TODO */
    /* enter the main program */
    call	_main
    /* exit */
    movq	%rax,%rdi
    movq	$0x2000001,%rax
    syscall

