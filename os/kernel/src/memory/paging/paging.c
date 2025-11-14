#include "paging.h"

#define PAGE_DIR_ADDR    0xFFFFF000
#define PAGE_TABLES_ADDR 0xFFC00000

#define KERNEL_PAGE_DIR_PHYS_ADDR 0x00006000
#define KERNEL_PAGE_TABLES_PHYS_ADDR 0x01100000

#define GET_PAGE_TABLE(vpn) ((page_table_entry*)(PAGE_TABLES_ADDR + ((vpn) / PAGES_PER_TABLE) * PAGE_SIZE))

// The locations of the structures that work along all directories thanks to recursive mapping
static const page_directory_entry* page_directory = (page_directory_entry*)PAGE_DIR_ADDR;
static const page_table_entry* page_tables = (page_table_entry*)PAGE_TABLES_ADDR;
// A variable to keep track of the virtual address (in the higher half) of the current pd
// of the current process
static page_directory_entry* current_page_directory = 
(page_directory_entry*)(KERNEL_PAGE_DIR_PHYS_ADDR + RELOCATION_OFFSET);

// page table addr can be NULL if the page is in the higher half (above 0xC0000000) 
void paging_map_page(uint32_t physical_page_index, uint32_t virtual_page_index, 
    bool only_kernel_mode, uintptr_t page_table_phys_addr)
{
    page_directory_entry* pd_entry = &page_directory[virtual_page_index / PAGES_PER_TABLE];

    // If the PDE is not present then initialize
    if (!pd_entry->present)
    {
        // get the page table's physical address
        if (virtual_page_index >= RELOCATION_OFFSET / PAGE_SIZE)
        {
            page_table_phys_addr = KERNEL_PAGE_TABLES_PHYS_ADDR + 
                (virtual_page_index / PAGES_PER_TABLE) * sizeof(page_table_entry) * PAGES_PER_TABLE;
        }
        memset(pd_entry, 0, sizeof(page_directory_entry));
        pd_entry->present = 1;
        pd_entry->read_write = 1;
        pd_entry->table_entry_address = page_table_phys_addr >> 12;
    }
    // Bit shifting is necessary to transfer
    page_table_entry* page_table = GET_PAGE_TABLE(virtual_page_index);
    page_table_entry* pt_entry = &page_table[virtual_page_index % 1024];

    // Initialize the page table entry
    pt_entry->physical_page_address = physical_page_index;
    pt_entry->user_supervisor = !only_kernel_mode;
    pt_entry->present = 1;
    pt_entry->read_write = 1;

    // Set pd_entry values
    pd_entry->user_supervisor |= !only_kernel_mode; // if the entry is already with user permissions then keep it

}

inline void paging_map_kernel_page(uint32_t physical_page_index, uint32_t virtual_page_index, 
    bool supervisor_permissions)
{
    paging_map_page(physical_page_index, virtual_page_index, supervisor_permissions, 0);
}

void paging_unmap_page(uint32_t virtual_page_index)
{
    page_table_entry* pte = get_pte(virtual_page_index);
    if (pte)
    {
        pte->present = 0;
    }
}

uintptr_t get_physical_address(void* virtual_address)
{
    uint32_t virtual_page_index = (uintptr_t)virtual_address / PAGE_SIZE;
    page_directory_entry* pd_entry = &page_directory[virtual_page_index / PAGES_PER_TABLE];

    if (!pd_entry->present)
    {
        return 0; // not mapped
    }

    page_table_entry* page_table = GET_PAGE_TABLE(virtual_page_index);
    page_table_entry* pt_entry = &page_table[virtual_page_index % 1024];
    return (pt_entry->physical_page_address << 12) + ((uintptr_t)virtual_address % PAGE_SIZE);
}

// Wrappers for physical memory
bool allocate_kernel_virtual_page(uint32_t virtual_page_index, bool supervisor_permissions)
{
    uint32_t physical_page_index = pmm_allocate_page();
    if (physical_page_index == 0)
        return false; // out of memory

    // page_table_entry* current_entry = get_pte(virtual_page_index);
    // if (current_entry && current_entry->present)
    //     return false; // Page already allocated
    
    paging_map_kernel_page(physical_page_index, virtual_page_index, supervisor_permissions);
    return true;
}

void deallocate_virtual_page(uint32_t virtual_page_index)
{
    page_table_entry* pte = get_pte(virtual_page_index);
    if (pte)
    {
        pmm_deallocate_page(pte->physical_page_address);
        pte->present = 0;
    }
}

// Helper functions
page_table_entry* get_pte(uint32_t virtual_page_index)
{
    return &page_tables[virtual_page_index];
}

inline void load_pd(page_directory_entry *pd)
{
    uintptr_t phys_addr = get_physical_address(pd);
    current_page_directory = pd;
    load_pd_phys_addr(phys_addr);
}

inline void load_pd_phys_addr(uintptr_t pd_phys_addr)
{
    asm volatile (
        "mov %0, %%cr3"
        :
        : "r"(pd_phys_addr)
        : "memory"
    );
}

inline page_directory_entry* get_current_pd()
{
    return current_page_directory;
}

struct page_directory_entry* get_kernel_pd()
{
    return (struct page_directory_entry*)(KERNEL_PAGE_DIR_PHYS_ADDR + RELOCATION_OFFSET);
}

inline uintptr_t get_current_pd_phys_addr() {
    uintptr_t cr3_reg;
    asm volatile ("mov %%cr3, %0" : "=r" (cr3_reg));
    return cr3_reg;
}
