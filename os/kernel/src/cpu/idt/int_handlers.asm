bits 32

%macro CREATE_ISR_NOERROR 1
    .first_int_handler_%1:
        cli
        push dword 0  ; push dummy error code
        push dword %1 ; push interrupts index
        jmp isr_common
%endmacro

%macro CREATE_ISR_ERROR 1
    .first_int_handler_%1:
        cli
                      ; error code is pushed by cpu
        push dword %1 ; push interrupts index
        jmp isr_common
%endmacro


%macro SET_ISR_NOERROR_RANGES 2
%assign i %1
%rep (%2 - %1 + 1)
    CREATE_ISR_NOERROR i
    %assign i i + 1
%endrep
%endmacro

%macro SET_ISR_ERROR_RANGES 2
%assign i %1
%rep (%2 - %1 + 1)
    CREATE_ISR_ERROR i
    %assign i i + 1
%endrep
%endmacro

section .text

; Set the first handlers for the interrupt exceptions
SET_ISR_NOERROR_RANGES 0, 7
SET_ISR_ERROR_RANGES   8, 8
SET_ISR_NOERROR_RANGES 9, 9
SET_ISR_ERROR_RANGES   10, 14
SET_ISR_NOERROR_RANGES 15, 16
SET_ISR_ERROR_RANGES   17, 17
SET_ISR_NOERROR_RANGES 18, 20
SET_ISR_ERROR_RANGES   21, 21
SET_ISR_NOERROR_RANGES 22, 28
SET_ISR_ERROR_RANGES   29, 30
SET_ISR_NOERROR_RANGES 31, 31
SET_ISR_NOERROR_RANGES 32, 255

extern isr_wrapper ; defined in idt.h

isr_common:
    pushad               ; pushes in order: eax, ecx, edx, ebx, esp, ebp, esi, edi

    xor eax, eax        ; push ds
    mov ax, ds
    push eax

    mov ax, 0x10        ; use kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    push esp            ; pass pointer to stack to C, so we can access all the pushed information
    call isr_wrapper
    add esp, 4

    pop eax             ; restore old ds
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popad                ; pop what we pushed with pusha
    add esp, 8          ; remove error code and interrupt number
    iret                ; will pop: cs, eip, eflags, ss, esp

section .data
align 4
%macro get_handler_address 1
dd .first_int_handler_%1
%endmacro

; this is an array of all ISRs to later put in the IDT
global first_int_handlers
first_int_handlers:
%assign i 0
%rep 256
    get_handler_address i
    %assign i i + 1
%endrep
