.global	_start
.section .text
_start:
    call main
spin:
    j spin
