global _start

section .text
_start:
    ; Write "Hello, World!" to stdout
    mov eax, 4         ; sys_write (Linux syscall number)
    mov ebx, 1         ; File descriptor: stdout
    mov ecx, message   ; Pointer to the message
    mov edx, message_len  ; Message length
    int 0x80           ; Call kernel

stop:
    jmp stop         

section .data
message db "Hello, World1!", 0xA
message_len equ $ - message