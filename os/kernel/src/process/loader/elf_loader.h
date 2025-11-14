#pragma once

#include "memory/paging/paging.h"
#include "process/manager/process_manager.h"

#define DEFAULT_STACK_PAGE_AMOUNT 0x100
#define USER_STACK_TOP (0xC0000000 - 0x1000)

uintptr_t elf_load_process(const uint8_t* elf_content, uint32_t elf_len, page_directory_entry* process_page_dir);
