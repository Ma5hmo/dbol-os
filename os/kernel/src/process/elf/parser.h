#pragma once
#include <stdbool.h>

#include "elf_header.h"

elf_hdr* elf_get_header(const uint8_t* content);
bool elf_is_valid_and_loadable(const uint8_t* content, uint32_t content_len);
elf_Phdr* elf_get_program_header(const uint8_t* content, uint32_t content_len);
elf_Shdr* elf_get_indexed_section_header(const uint8_t* content, uint32_t content_len, uint8_t index);
uint32_t elf_get_size_in_mem(const uint8_t* content, uint32_t content_len);
