set disassemble-next-line on
set style address foreground red
b _entry
target remote 127.0.0.1:1234
c