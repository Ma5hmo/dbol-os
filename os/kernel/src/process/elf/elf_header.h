#pragma once

#include <stdint.h>

#define EI_NIDENT 16

typedef struct elf_hdr_t {
    uint8_t       e_ident[EI_NIDENT]; // Array of bytes that specifies how to interpret the file, including the magic number
    uint16_t      e_type;      // Identifies the object file type
    uint16_t      e_machine;   // Specifies the required architecture for the file
    uint32_t      e_version;   // Identifies the version of the ELF specification to which the file conforms
    uint32_t      e_entry;     // Virtual address to the program's entry point
    uint32_t      e_phoff;     // Offset in the file where the program header table is located
    uint32_t      e_shoff;     // Offset in the file where the section header table is located
    uint32_t      e_flags;     // Processor-specific flags associated with the file
    uint16_t      e_ehsize;    // Size of this header
    uint16_t      e_phentsize; // Size of a program header table entry
    uint16_t      e_phnum;     // Number of entries in the program header table
    uint16_t      e_shentsize; // Size of a section header table entry
    uint16_t      e_shnum;     // Number of entries in the section header table
    uint16_t      e_shstrndx;  // Index of the section header table entry that contains the section names
} elf_hdr;


typedef enum elf_type_of_segment
{
    elf_type_of_segment_load = 1,
    elf_type_of_segment_dynamic = 2,
    elf_type_of_segment_iterpreter = 3,
    elf_type_of_segment_note = 4,

    _elf_type_of_segment_extend_to_32_bit = 0xFFFFFFFF
} elf_type_of_segment;

typedef struct elf_program_header_flags
{
    uint8_t executable : 1;
    uint8_t writable : 1;
    uint8_t readable : 1;
    uint8_t reserved : 5;
} __attribute__((packed)) elf_program_header_flags;

typedef struct elf_Phdr_t {
	elf_type_of_segment p_type;
	uint32_t p_offset;
	uint32_t p_vaddr;
	uint32_t p_paddr;
	uint32_t p_filesz;
	uint32_t p_memsz;
	elf_program_header_flags p_flags;
	uint32_t p_align;
} elf_Phdr;

typedef enum elf_section_header_type
{
    elf_section_header_type_unused = 0x00,
    elf_section_header_type_program_data = 0x01,
    elf_section_header_type_symbol_table = 0x02,
    elf_section_header_type_string_table = 0x03,
    elf_section_header_type_relocation_entries_with_addens = 0x04,
    elf_section_header_type_symbol_hash_table = 0x05,
    elf_section_header_type_dynamic_linking_information = 0x06,
    elf_section_header_type_notes = 0x07,
    elf_section_header_type_bss = 0x08,
    elf_section_header_type_relocation_entries_without_addens = 0x09,
    elf_section_header_type_reserved = 0x0A,
    elf_section_header_type_dynamic_linker_symbol_table = 0x0B,
    elf_section_header_type_array_of_constructors = 0x0E,
    elf_section_header_type_array_of_destructors = 0x0F,
    elf_section_header_type_array_of_preconstructors = 0x10,
    elf_section_header_type_section_group = 0x11,
    elf_section_header_type_extended_section_indices = 0x12,
    elf_section_header_type_number_of_defined_types = 0x13,

    _elf_section_header_type_extend_to_32_bit = 0xFFFFFFFF
} elf_section_header_type;

typedef struct elf_section_header_flags
{
    uint8_t writable : 1;
    uint8_t occupies_memory_during_execution : 1;
    uint8_t executable : 1;
    uint8_t might_be_merged : 1;
    uint8_t contains_null_terminated_strings : 1;
    uint8_t contains_sht_index : 1;
    uint8_t preserve_order_after_combining : 1;
    uint8_t non_standard_handlind_required : 1;
    uint8_t section_is_member_of_group : 1;
    uint8_t section_hold_thread_local_data : 1;
    uint32_t reserved : 22;
} __attribute__((packed)) elf_section_header_flags;

typedef struct elf_Shdr_t
{
    uint32_t name_offset;
    elf_section_header_type type;
    elf_section_header_flags flags;
    uint32_t virtual_address;
    uint32_t file_offset;
    uint32_t size;
    uint32_t associated_section_index;
    uint32_t extra_information;
    uint32_t alignment;
    uint32_t entry_size;
} elf_Shdr;