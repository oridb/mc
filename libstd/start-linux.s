.globl _start
_start:
    /* turn args into a slice */
    /* TODO */
    /* enter the main program */
    call	main
    /* exit */
    movq	%rax,%rbx
    movq	$60,%rax
    sysenter

