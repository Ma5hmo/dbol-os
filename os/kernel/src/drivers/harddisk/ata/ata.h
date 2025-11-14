#pragma once
#include <stdint.h>

/*
This file contains the ata pio actions implementations

- Read from disk
- Write to disk
- Wait 400ns 

*/

#define ATA_DATA 0x1F0
#define ATA_SECTOR_COUNT 0x1F2
#define ATA_SECTOR_LOW 0x1F3
#define ATA_SECTOR_MID 0x1F4
#define ATA_SECTOR_HIGH 0x1F5
#define ATA_SELECT_DRIVE 0x1F6
#define ATA_CMD_STATUS 0x1F7

#define SECTOR_SIZE 512

typedef struct ata_drive {
    uint16_t cylinders;
    uint16_t heads;
    uint16_t sectors_per_track;
    uint64_t total_sectors;
    char model[41];
    uint8_t is_present;
} ata_drive;

void ata_init();
ata_drive* get_main_drive();

int ata_read(uint32_t sector_number, uint8_t sector_count, void* buffer);
int ata_write(uint32_t sector_number, uint8_t sector_count, const void* buffer);
static void wait_400ns();