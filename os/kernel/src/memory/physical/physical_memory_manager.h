#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "multiboot.h"

typedef enum {
    PMM_SUCCESS = 0,
    PMM_ERROR_INVALID_PARAMS,
    PMM_ERROR_OUT_OF_MEMORY,
    PMM_ERROR_ALREADY_INITIALIZED
} pmm_status_t;

static enum PAGE_STATUS {
    MEMORY_FREE = 0,
    MEMORY_OCCUPIED = 1
};


typedef struct {
    uint32_t* bitmap;
    uint32_t bitmap_size;  // in uint32_t's
    uint32_t max_pages;
    uint32_t used_pages;
} pmm_info_t;

pmm_status_t pmm_init(multiboot_info_t *mbi);

void set_bit_status(uint32_t bit_index, enum PAGE_STATUS status);
void set_occupied(uint32_t bit_index);
void set_free(uint32_t bit_index);
bool check_if_occupied(uint32_t bit_index);

void pmm_init_region(uint32_t address, uint32_t length);
void pmm_clear_region(uint32_t address, uint32_t length);
int find_first_free_block();
uint32_t pmm_allocate_page();
void pmm_deallocate_page(uint32_t page_index);