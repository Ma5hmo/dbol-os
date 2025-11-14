#include "heap.h"
#include "../paging/paging.h"
#include "../../drivers/vga/vga.h"

#define BLOCK_SIZE(block) ((uintptr_t)block->next - (uintptr_t)block - sizeof(heap_entry))
#define ALIGN(size, alignment) (((size) + (alignment)-1) & ~((alignment)-1))
#define MIN_BLOCK_SIZE 4

static const uintptr_t heap_start = KERNEL_CODE_END;
static uintptr_t curr_heap_end = KERNEL_CODE_END;
static heap_entry* last_entry;

static void *heap_find_first_free_space(size_t wanted_size);
static void free_block(heap_entry *block);
static bool allocate_heap_page();
static void deallocate_last_heap_page();

bool heap_init()
{
    if (!allocate_heap_page())
    {
        return false;
    }

    // Set up the first block
    heap_entry* first = (heap_entry *)heap_start;
    last_entry = (heap_entry*)(curr_heap_end - sizeof(heap_entry));
    last_entry->next = NULL;
    last_entry->prev = first;
    last_entry->is_free = false;

    first->next = last_entry;
    first->prev = NULL;
    first->is_free = true;
    return true;
}

static void *heap_find_first_free_space(size_t wanted_size)
{
    if (wanted_size == 0 || wanted_size > KERNEL_HEAP_END - heap_start)
    {
        return NULL;
    }
    heap_entry *curr = (heap_entry *)heap_start;
    wanted_size = ALIGN(wanted_size, 4);
    int i = 0;
    // Find the first block with enough space
    while (!curr->is_free || curr->next == NULL || BLOCK_SIZE(curr) < wanted_size)
    {
        if (curr->next == NULL || (uintptr_t)curr->next >= curr_heap_end) // This entry marks the end of the heap
        {
            // Not enough space, allocate another page
            if (!allocate_heap_page())
            {
                return NULL;
            }
            
            if (curr->prev && curr->prev->is_free)
            {
                // Update the size of the last entry
                curr = curr->prev;
            }
            else
            {
                // Use this entry (the last one) as an entry for the new page's block
                curr->is_free = true;
            }
            
            // Define the new last_entry
            curr->next = (heap_entry*)(curr_heap_end - sizeof(heap_entry));

            last_entry = curr->next;
            last_entry->next = NULL;
            last_entry->prev = curr;
            last_entry->is_free = false;

            continue; // Try the new entry again, dont go to the next one           
        }

        // Check the next block
        curr = curr->next;
        i++;
    }

    // Need to split the block to two different blocks
    if (BLOCK_SIZE(curr) >= wanted_size + sizeof(heap_entry) + MIN_BLOCK_SIZE)
    {
        heap_entry *curr_end = curr->next;
        curr->next = (heap_entry*)((uintptr_t)curr + wanted_size + sizeof(heap_entry));
    
        curr->next->next = curr_end;
        curr->next->prev = curr;
        curr->next->is_free = true;

        curr_end->prev = curr->next;
    }

    // Keep the current size of the block
    curr->is_free = false;

    return (void *)((uintptr_t)curr + sizeof(heap_entry));
}

static void free_block(heap_entry *block)
{
    if ((uintptr_t)block < heap_start || (uintptr_t)block > KERNEL_HEAP_END || block->next == NULL)
    {
        return;
    }

    // Last block
    if (block->next->next == NULL)
    {
        last_entry = block;
        last_entry->next = NULL;
        last_entry->is_free = true;
        // Is the block on the last page too?
        if (ALIGN((uintptr_t)block, PAGE_SIZE) < ALIGN(curr_heap_end, PAGE_SIZE))
        {
            deallocate_last_heap_page();
        }
        return;
    }
    
    // Combine the previous block with it
    if (block->prev && block->prev->is_free)
    {
        block->prev->next = block->next;
        return;
    }

    // Combine the next block with it
    if (block->next->is_free)
    {
        block->next = block->next->next;
    }
    block->is_free = true;
}

static bool allocate_heap_page()
{
    if (curr_heap_end >= KERNEL_HEAP_END)
    {
        return false;
    }
    if (allocate_kernel_virtual_page(curr_heap_end / PAGE_SIZE, true))
    {
        curr_heap_end += PAGE_SIZE;
        return true;
    }
    return false;
}

static void deallocate_last_heap_page()
{
    const uintptr_t last_page_start_addr = curr_heap_end - PAGE_SIZE;
    deallocate_virtual_page(last_page_start_addr / PAGE_SIZE);
    curr_heap_end -= PAGE_SIZE;
}

inline void* kmalloc(size_t size)
{
    return heap_find_first_free_space(size);
}

void* kmalloc_pages(size_t page_amount)
{
    if (page_amount == 0 || page_amount > (KERNEL_HEAP_END - heap_start) / PAGE_SIZE)
    {
        return NULL;
    }
    // this is the page we will return
    for (int i = 0; i < page_amount + 1; i++)
    {
        if (!allocate_heap_page())
        {
            for (int j = 0; j < i; j++)
            {
                deallocate_last_heap_page();
            }
            return NULL;
        }
    }
    heap_entry* returned_entry =
            (heap_entry*)(curr_heap_end - ((page_amount + 1) * PAGE_SIZE) - sizeof(heap_entry));
    returned_entry->is_free = false;

    if (!last_entry->prev->is_free)
    {
        last_entry->prev->next = returned_entry;
        returned_entry->prev = last_entry->prev;
    }
    else
    {
        // discard the last block
        last_entry->prev->prev->next = returned_entry;
        returned_entry->prev = last_entry->prev->prev;
    }

    // the new last free block on the heap
    returned_entry->next = (heap_entry*)((uintptr_t)returned_entry + page_amount * PAGE_SIZE + sizeof(heap_entry));
    returned_entry->next->is_free = true;

    last_entry = (heap_entry*)(curr_heap_end - sizeof(heap_entry));
    last_entry->is_free = false;
    last_entry->prev = returned_entry->next;
    returned_entry->next->next = last_entry;
    returned_entry->next->prev = returned_entry;
    return (void*)((uintptr_t)returned_entry + sizeof(heap_entry));
}

inline void kfree(void *addr)
{
    // free the block using the heap entry's address
    free_block((heap_entry *)((uintptr_t)addr - sizeof(heap_entry)));
}

inline int get_heap_end()
{
    return curr_heap_end;
}
