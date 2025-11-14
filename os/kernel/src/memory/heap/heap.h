#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Decided to use a set place instead of externing the kernel's
// end like this
// extern char _kernel_end;

#define KERNEL_CODE_END 0xC1800000
#define KERNEL_HEAP_END 0xFFFFF000

typedef struct heap_entry
{
    bool is_free;
    struct heap_entry* next;
    struct heap_entry* prev;
} heap_entry;

bool heap_init();

void* kmalloc(size_t size);
void* kmalloc_pages(size_t page_amount);

void kfree(void* buffer);

int get_heap_end();