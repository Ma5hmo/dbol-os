section .data
    filename db "/proc1", 0     ; File name to open

section .bss
    file_handle resd 1          ; File descriptor
    buffer resb 1024            ; Buffer for file contents
    buffer_len equ 1024         ; Buffer size

section .text
    global _start

_start:
    ; Read from stdin
     ; Read from file
    mov eax, 3                  ; sys_read (Linux syscall number)
    mov ebx, 0      ; File descriptor
    mov ecx, buffer             ; Buffer to store data
    mov edx, 5         ; Maximum bytes to read
    int 0x80                    ; Call kernel
    
    ; Save number of bytes read
    mov edx, eax                ; eax contains number of bytes read
    
    ; Write the data to stdout
    mov eax, 4                  ; sys_write (Linux syscall number)
    mov ebx, 1                  ; File descriptor: stdout
    mov ecx, buffer             ; Pointer to the buffer
    ; edx already contains the number of bytes to write
    int 0x80       

    stay:
        jmp stay