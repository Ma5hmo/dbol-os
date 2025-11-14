#pragma once
#include "memory/physical/physical_memory_manager.h"

#define PAGE_SIZE    0x1000
#define KERNEL_PHYS_ADDR 0x00100000
#define KERNEL_VIRT_ADDR 0xC0100000
#define RELOCATION_OFFSET (KERNEL_VIRT_ADDR - KERNEL_PHYS_ADDR)

typedef struct page_directory_entry
{
    uint8_t present : 1;
    uint8_t read_write : 1;
    uint8_t user_supervisor : 1;
    uint8_t write_through : 1;
    uint8_t cache_disabled : 1;
    uint8_t accessed : 1;
    uint8_t reserved : 1;
    uint8_t page_size : 1;
    uint8_t global : 1;

    uint8_t available : 3;
    uint32_t table_entry_address : 20;
} __attribute__((packed)) page_directory_entry;


typedef struct page_table_entry
{
    uint8_t present : 1;
    uint8_t read_write : 1;
    uint8_t user_supervisor : 1;
    uint8_t write_through : 1;
    uint8_t cache_disabled : 1;
    uint8_t accessed : 1;
    uint8_t reserved : 1;
    uint8_t dirty : 1;
    uint8_t global : 1;

    uint8_t available : 3;
    uint32_t physical_page_address : 20;
} __attribute__((packed)) page_table_entry;

#define PAGES_PER_TABLE 1024
#define PAGES_PER_DIR	1024

// Helper functions
page_table_entry* get_pte(uint32_t virtual_page_index);

// Memory mapping in paging
void paging_map_page(uint32_t physical_page_index, uint32_t virtual_page_index, bool user_permission,
    uintptr_t page_table_phys_addr);
void paging_map_kernel_page(uint32_t physical_page_index, uint32_t virtual_page_index, 
    bool user_permission);
void paging_unmap_page(uint32_t virtual_page_index);

// Physical memory wrappers
uintptr_t get_physical_address(void* virtual_address);
bool allocate_kernel_virtual_page(uint32_t virtual_page_index, bool supervisor_permissions);
void deallocate_virtual_page(uint32_t virtual_page_index);

// Basic functions in paging
struct page_directory_entry* get_current_pd();
struct page_directory_entry* get_kernel_pd();
void load_pd(struct page_directory_entry* pd);
void load_pd_phys_addr(uintptr_t pd_phys_addr);
uintptr_t get_current_pd_phys_addr();
