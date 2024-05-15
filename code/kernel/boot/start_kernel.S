	.global	_entry
.section .text

_entry:
    la sp, stack0
    li a0, 1024*4
	csrr a1, mhartid
    addi a1, a1, 1
    mul a0, a0, a1
    add sp, sp, a0
	# jump to start_kernel() in start.c
    call start_kernel
spin:
        j spin

