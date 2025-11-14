#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ctype.h"

// Define constants for FAT16
#define FAT16_SIGNATURE 0xAA55
#define FAT16_TYPE "FAT16"

#define FAT16_CLUSTER_CHAIN_END 0xFFF8
#define FAT16_BAD_CLUSTER 0xFFF7
#define FAT16_FREE_CLUSTER 0x0000

#define FAT16_FILENAME_SIZE 11 // 8.3 filename (8 bytes name, 3 bytes extension)

typedef enum FAT16_FileAttribute {
    FAT_ATTR_READ_ONLY = 0x01,
    FAT_ATTR_HIDDEN = 0x02,
    FAT_ATTR_SYSTEM = 0x04,
    FAT_ATTR_VOLUME_ID = 0x08,
    FAT_ATTR_DIRECTORY = 0x10,
    FAT_ATTR_ARCHIVE = 0x20,
    FAT_ATTR_LFN = FAT_ATTR_READ_ONLY | FAT_ATTR_HIDDEN | FAT_ATTR_SYSTEM | FAT_ATTR_VOLUME_ID
} FAT16_FileAttribute;

// In-memory structure for FAT16 metadata
typedef struct {
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t num_fats;
    uint16_t root_dir_entries;
    uint16_t total_sectors;
    uint16_t sectors_per_fat;
    uint32_t root_dir_start;  // Starting sector of the root directory
    uint32_t data_start;      // Starting sector of the data region
} FAT16_FS;

void ata_init(const char* filename);
int ata_read(uint32_t sector_number, uint8_t sector_count, uint16_t* buffer);
int ata_write(uint32_t sector_number, uint8_t sector_count, const uint16_t* buffer);


// File and directory entry structure
typedef struct FAT16_DirEntry {
    char name[FAT16_FILENAME_SIZE];
    uint8_t attr;          // File attributes
    uint8_t case_attr;          
    uint8_t creation_time_ms;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t reserved;
    uint16_t last_modification_time;
    uint16_t last_modification_date;
    uint16_t start_cluster; // Starting cluster
    uint32_t file_size;     // File size in bytes
} FAT16_DirEntry;

bool fat_init();

typedef enum ReturnCode {
 SUCCESS,
 GENERAL_ERROR,
 DIR_NOT_FOUND,
 FILE_NOT_FOUND,
 FILE_ALREADY_EXISTS,
 DIR_NAME_EXCEEDS_LENGTH,
 FILE_NAME_EXCEEDS_LENGTH,
 INVALID_FILE_NAME,
 DIRECTORY_ISNT_EMPTY,
 CANT_ALLOCATE_SPACE
} ReturnCode;

typedef struct FileData {
    FAT16_DirEntry file_entry;
    FAT16_DirEntry parent_entry;
} FileData;


// file system API
uint32_t fat_create_file(const char *path);
uint32_t fat_create_directory(const char *path);
uint32_t fat_rename(const char *path, const char *new_name);
FileData fat_get_file_data(const char *path);
FileData fat_get_dir_data(const char *path);
uint32_t fat_delete_file(const char *path);
uint32_t fat_delete_dir(const char *path);
int32_t fat_read(FAT16_DirEntry* file, uint32_t offset, uint32_t size, void* buffer);
int32_t fat_write(FAT16_DirEntry* file, FAT16_DirEntry* mother_dir, uint32_t offset, uint32_t size, const void* buffer);