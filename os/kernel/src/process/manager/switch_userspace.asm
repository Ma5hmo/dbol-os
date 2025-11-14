global jump_usermode
jump_usermode:
    ; set the segments to be ring 4
    mov esp, eax

    mov ax, 0x23
    mov ds, ax
    mov es, ax 
    mov fs, ax 
    mov gs, ax

    ; pop all registers
    popad
    sti
    iret ; pop eip, cs, flags, esp, ss and jump to the usermode code