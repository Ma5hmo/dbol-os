global jump_kernelmode
jump_kernelmode:
    ; set the segments to be ring 4
    mov esp, eax

    mov ax, 0x10
    mov ds, ax
    mov es, ax 
    mov fs, ax 
    mov gs, ax

    ; pop all registers
    popad
    iret ; pop eip, cs, flags, esp, ss and jump to the usermode code