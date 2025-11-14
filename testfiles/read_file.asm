section .data
    filename db "/proc1", 0     ; File name to open

section .bss
    file_handle resd 1          ; File descriptor
    buffer resb 1024            ; Buffer for file contents
    buffer_len equ 1024         ; Buffer size

section .text
    global _start

_start:
    ; Open the file for reading
    mov eax, 5                  ; sys_open (Linux syscall number)
    mov ebx, filename           ; Pointer to filename
    mov ecx, 0                  ; O_RDONLY (read-only)
    int 0x80                    ; Call kernel
    
    ; Save file descriptor
    mov [file_handle], eax
    
    ; Read from file
    mov eax, 3                  ; sys_read (Linux syscall number)
    mov ebx, 0      ; File descriptor
    mov ecx, buffer             ; Buffer to store data
    mov edx, buffer_len         ; Maximum bytes to read
    int 0x80                    ; Call kernel
    
    ; Save number of bytes read
    mov edx, eax                ; eax contains number of bytes read
    
    ; Write the data to stdout
    mov eax, 4                  ; sys_write (Linux syscall number)
    mov ebx, 1                  ; File descriptor: stdout
    mov ecx, buffer             ; Pointer to the buffer
    ; edx already contains the number of bytes to write
    int 0x80                    ; Call kernel
    
    ; Close the file
    mov eax, 6                  ; sys_close (Linux syscall number)
    mov ebx, [file_handle]      ; File descriptor
    int 0x80                    ; Call kernel
    
    stay:
        jmp stay