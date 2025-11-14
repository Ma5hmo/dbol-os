#include "elf_loader.h"
#include "process/elf/elf_header.h"
#include "process/elf/parser.h"
#include "memory/physical/physical_memory_manager.h"
#include "memory/heap/heap.h"

static bool allocate_multiple_pages(uint32_t starting_page_index, uint32_t page_count,
    bool supervisor_permissions, uintptr_t page_tables_phys_addr)
{
    for (uint32_t i = 0; i < page_count; i++)
    {
        uint32_t physical_page_index = pmm_allocate_page();
        if (physical_page_index == 0)
        {
            for (uint32_t j = 0; j < i; j++)
            {
                deallocate_virtual_page(starting_page_index + j);
            }
            return false;
        }
        paging_map_page(physical_page_index, starting_page_index + i, supervisor_permissions,
                page_tables_phys_addr);
    }
    return true;
}

uintptr_t elf_load_process(const uint8_t* elf_content, uint32_t elf_len,
        page_directory_entry* process_page_dir)
{
    elf_hdr* header = elf_get_header(elf_content);
    size_t process_size = elf_get_size_in_mem(elf_content, elf_len);
    
    struct page_table_entry* process_page_tables = 
        (page_table_entry*)kmalloc_pages(process_size / PAGE_SIZE / PAGES_PER_DIR + 1);
    uintptr_t page_tables_phys_addr = get_physical_address(process_page_tables);

    // allocate the virtual pages for the process
    if (!allocate_multiple_pages(header->e_entry / PAGE_SIZE,
        process_size / PAGE_SIZE + 2, false, page_tables_phys_addr)) 
    {
        kfree(process_page_tables);
        return 0;
    }
    
    // allocate the process's stack`
    if (!allocate_multiple_pages(USER_STACK_TOP / PAGE_SIZE - DEFAULT_STACK_PAGE_AMOUNT, 
        DEFAULT_STACK_PAGE_AMOUNT, false, page_tables_phys_addr))
    {
        // deallocate the process's previously allocated pages
        for (uint32_t i = 0; i < process_size / PAGE_SIZE + 1; i++)
        {
            deallocate_virtual_page(header->e_entry / PAGE_SIZE + i);
        }
        kfree(process_page_tables);
        return 0;
    }


    
    for (uint32_t i = 0; i < header->e_shnum; i++)
    {
        elf_Shdr *section_header = elf_get_indexed_section_header(elf_content, elf_len, i);
        if (section_header->size == 0 || !section_header->flags.occupies_memory_during_execution)
        {
            continue;
        }

        // memcpy the section to the newly mapped pages
        if (section_header->type == elf_section_header_type_program_data)
        {
            memcpy((void*)section_header->virtual_address, &elf_content[section_header->file_offset],
                section_header->size);
        }
        else if (section_header->type == elf_section_header_type_bss)
        {
            memset((void*)section_header->virtual_address, 0, section_header->size);
        }
    }

    return process_size;
}
