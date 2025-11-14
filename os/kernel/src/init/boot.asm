bits 32		;nasm directive

; Kernel Memory Layout:
; 0 - 0xC0006000   	  0xC0006000 - 0xC0008000   0xC0008000 - 0xC000FFFF   
; Bios Reserved       Page Directory	        Bios & kernel
; --------------------------------------------------------------------------------
; 0xC0100000 
;

%define PAGE_DIRECTORY_BASE 0x00006000
%define PAGE_TABLES_BASE    0x01100000

section .multiboot
	;multiboot spec
	align 4
	dd 0x1BADB002			;magic
	dd 0x00000000			;flags
	dd - (0x1BADB002 + 0x00000000)	;checksum. m+f+c should be zero
	dd 0, 0, 0, 0, 0

	; graphics
	dd 0
	dd 800
	dd 600
	dd 32

KERNEL_STACK_SIZE equ 0x400000   
section .bss
align 4
stack_bottom:
	RESB KERNEL_STACK_SIZE
stack_top:

section .boot
global _start
_start:
	push ebx
	call init_paging
	pop ebx
	jmp higher_kernel
	hlt


%define PAGE_DIRECTORY_BASE 0x00006000
%define PAGE_TABLES_BASE    0x01100000
; all of the page tables from 0xc0000000 to 0xfffff000 are mapped for the kernel and also identity mapped
%define PAGE_TABLE_COUNT    6

init_paging:
	call clear_page_directory
	call clear_page_tables
	call create_identity_tables
	call create_pd

	; put the page dir inside the cr3 register
	mov eax, PAGE_DIRECTORY_BASE
	mov cr3, eax

	mov eax, cr0
	or eax, 0x80000000
	mov cr0, eax

	ret

create_pd:
	%macro page_directory_definition 3
    mov eax, %1
	or eax, 3
    mov [%2 + %3], eax
    %endmacro

	; init the pd with ptrs to the ptables identity map
	%assign pd_index 0 
    %rep    PAGE_TABLE_COUNT
    page_directory_definition PAGE_TABLES_BASE + (pd_index * 0x1000), PAGE_DIRECTORY_BASE, pd_index * 4
    %assign pd_index pd_index+1 
    %endrep

	; same map but to higher half kernel
	%assign pd_index 0 
    %rep    PAGE_TABLE_COUNT
    page_directory_definition PAGE_TABLES_BASE + (pd_index * 0x1000), PAGE_DIRECTORY_BASE + 0xC00, pd_index * 4
    %assign pd_index pd_index+1 
    %endrep

	; implement recursive mapping (map the last page directory entry to the page directory itself)
	; this is used in order to set a virtual address for all of the page tables
	page_directory_definition PAGE_DIRECTORY_BASE, PAGE_DIRECTORY_BASE, 1023 * 4

	ret

create_identity_tables:
	mov eax, 0 ; index
	mov ebx, 0 ; physical address	

	.init_pt_loop:	
	mov ecx, ebx
	or ecx, 3 ; add the read/write and present flags

	; mov the entry value in to TABLE_BASE + eax(32 bit aligned, which is *4 in terms of bytes)
	mov [PAGE_TABLES_BASE + eax*4], ecx 

	; Increment index
	add ebx, 0x1000
    inc eax

    ; Leave loop if we initialzied all entries
    cmp eax, PAGE_TABLE_COUNT * 0x1000
    jl .init_pt_loop

	ret

clear_page_tables:
    ; Index
    mov eax, 0

    .clear_page_tables_loop:
    ; Clear entry
    mov [PAGE_TABLES_BASE + eax], byte 0

    ; Increment index
    inc eax

    ; Leave loop if we cleared all entries
    cmp eax, 0x400000
    jl .clear_page_tables_loop

    ret

clear_page_directory:
    ; Index
    mov eax, 0

    .clear_page_directory_loop:
    ; Clear entry
    mov [PAGE_DIRECTORY_BASE + eax], byte 0

    ; Increment index
    inc eax

    ; Leave loop if we cleared all entries
    cmp eax, 0x1000
    jl .clear_page_directory_loop

    ret

extern kmain
section .text
higher_kernel:
	mov esp, stack_top
	push ebx
	xor ebp, ebp
	call kmain

HaltKernel:
	hlt
	jmp HaltKernel

section .data
align 4096


