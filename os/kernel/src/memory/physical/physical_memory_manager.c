#include "physical_memory_manager.h"

#define PAGE_SIZE           4096            // 4 KB pages
#define PAGE_SIZE_BITS      12             // 2^12 = 4096
#define MAX_PHYSICAL_MEMORY (4ULL * 1024 * 1024 * 1024)  // 4 GB
#define PAGES_PER_DWORD     32             // bits in uint32_t
#define BITMAP_SIZE         ((MAX_PHYSICAL_MEMORY / PAGE_SIZE + PAGES_PER_DWORD - 1) / PAGES_PER_DWORD)

static pmm_info_t pmm_info = {0};

static bool is_initialized = false;

pmm_status_t pmm_init(multiboot_info_t *mbi) {
    // Chekc if already initialized
    if (!mbi || is_initialized) {
        return PMM_ERROR_ALREADY_INITIALIZED;
    }

    // Calculate the total memory from the multiboot_info
    uint64_t total_memory = 1024 + mbi->mem_lower + (uint64_t)mbi->mem_upper*64;
    total_memory *= 1024; // Convert to bytes

    // Calculate bitmap dimentions
    pmm_info.max_pages = total_memory / PAGE_SIZE;
    pmm_info.bitmap_size = pmm_info.max_pages / PAGES_PER_DWORD;

    // Allocate a bitmap
    static uint32_t bitmap[BITMAP_SIZE];
    pmm_info.bitmap = bitmap;
    pmm_info.used_pages = pmm_info.max_pages;

    // Set all memory to be occupied
    memset(pmm_info.bitmap, 0xff, BITMAP_SIZE);

    
    // Get the start and end of the memory map
    multiboot_memory_map_t* mmap = (multiboot_memory_map_t*) mbi->mmap_addr;
    multiboot_memory_map_t* mmap_end = (multiboot_memory_map_t*) (mbi->mmap_addr + mbi->mmap_length);

    // Iterate through memory map entries
    for (; mmap < mmap_end; mmap = (multiboot_memory_map_t*) ((uint32_t)mmap + mmap->size + sizeof(mmap->size))) {
        if (mmap->type == 1)
        {
            pmm_init_region(mmap->addr_low, mmap->len_low);
        }
    }

    // Mark first 24 megabyte as reserved
    pmm_clear_region(0x0, 0x1800000);

    is_initialized = true;
    set_occupied(0);
    return PMM_SUCCESS;
}

// Initialzie a memory region to available
void pmm_init_region(uint32_t address, uint32_t length) {
    uint32_t start_page = address / PAGE_SIZE;
    uint32_t num_pages = length / PAGE_SIZE;

    for (uint32_t i = 0; i < num_pages; i++) {
        set_free(start_page + i);
    }

    // Update the number of used pages
    pmm_info.used_pages -= num_pages;
}

// deinitialzie a memory region to unavailable
void pmm_clear_region(uint32_t address, uint32_t length) {
    uint32_t start_page = address / PAGE_SIZE;
    uint32_t num_pages = length / PAGE_SIZE;

    for (uint32_t i = 0; i < num_pages; i++) {
        set_occupied(start_page + i);
    }

    // Update the number of used pages
    pmm_info.used_pages += num_pages;
}

// find first free block
int find_first_free_block() {
    if (pmm_info.used_pages == pmm_info.max_pages)
        return -1;

    // loop through each 32bit size entry in the map
    for (int entry=0; entry < pmm_info.max_pages / 32; ++entry) {
        // check if entire entry is occupied
        if (pmm_info.bitmap[entry] == 0xffffffff)
            {
                continue;
            }
        // loop through each bit
        for (int bit=0; bit < 32; ++bit) {
            uint32_t block_index = entry * 32 + bit;

            if (block_index >= pmm_info.max_pages)
                break;

            if (!check_if_occupied(block_index))
                return block_index;
        }
    }

    return -1;
}

// allocate a block (page frame)
uint32_t pmm_allocate_page() {
    uint32_t page_index = find_first_free_block();

    if (page_index == -1)
        return 0; // out of memory

    set_occupied(page_index);
    ++pmm_info.used_pages;

    return page_index;
}

// free block (page frame)
void pmm_deallocate_page(uint32_t page_index) {
    --pmm_info.used_pages;
    set_free(page_index);
}

void set_bit_status(uint32_t bit_index, enum PAGE_STATUS status) {
    int arr_index = bit_index / 32;
    int shift_amount = bit_index % 32;
    int bit_mask = 1 << shift_amount;
    
    if (status == MEMORY_FREE)
        pmm_info.bitmap[arr_index] &= bit_mask;
    else    
        pmm_info.bitmap[arr_index] |= bit_mask;
}
void set_occupied(uint32_t bit_index)
{
    set_bit_status(bit_index, MEMORY_OCCUPIED);
}
void set_free(uint32_t bit_index)
{
    set_bit_status(bit_index, MEMORY_FREE);
}

bool check_if_occupied(uint32_t bit_index) {
    int arr_index = bit_index / 32;
    int shift_amount = bit_index % 32;
    int bit_mask = 1 << shift_amount;

    return pmm_info.bitmap[arr_index] & bit_mask;
}