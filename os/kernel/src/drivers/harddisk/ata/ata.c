#include "ata.h"
#include "util/io/io.h"
#include "drivers/vga/vga.h"



ata_drive main_driver = {0};

void ata_init()
{
    uint16_t data_buffer[256] = {0};
    io_out_byte(ATA_SELECT_DRIVE, (uint8_t)(0xA0));   // 0xA0 for master drive

    // Send LBA to be 0 bits 0-7, 8-15, and 16-23
    io_out_byte(ATA_SECTOR_LOW, 0);
    io_out_byte(ATA_SECTOR_MID, 0);
    io_out_byte(ATA_SECTOR_HIGH, 0);

    // Send the IDENTIFY command
    io_out_byte(ATA_CMD_STATUS, 0xEC);    

    uint8_t drive_status = 0;
    drive_status = io_in_byte(ATA_CMD_STATUS);

    if (drive_status == 0) // Drive doesnt exist
    {
        main_driver.is_present = 0;
        return;
    }

    // Read data of IDENTIFY
    for (int i = 0; i < 256; i++)
    {
        data_buffer[i] = io_in_word(ATA_DATA);
    }

     // Parse IDENTIFY data
    main_driver.is_present = 1;
    main_driver.cylinders = data_buffer[1];
    main_driver.heads = data_buffer[3];
    main_driver.sectors_per_track = data_buffer[6];
    main_driver.total_sectors =
        (uint32_t)data_buffer[60] | ((uint32_t)data_buffer[61] << 16);

    // Copy model number
    for (int i = 0; i < 20; i++) {
        main_driver.model[i * 2] = data_buffer[27 + i] & 0xFF;
        main_driver.model[i * 2 + 1] = data_buffer[27 + i] >> 8;
    }
    main_driver.model[40] = '\0';
}

inline ata_drive* get_main_drive()
{
    return &main_driver;
}

int ata_read(uint32_t sector_number, uint8_t sector_count, void* buffer)
{
    if (!main_driver.is_present)
        return 1;
    io_out_byte(ATA_SELECT_DRIVE, (uint8_t)((sector_number >> 24) | 0xE0));   // 0xE0 is for LBA(sector_number) mode
    io_out_byte(ATA_SECTOR_COUNT, sector_count);                    // Output the amount of sectors to read

    // Send LBA(sector_number) bits 0-7, 8-15, and 16-23
    io_out_byte(ATA_SECTOR_LOW, (uint8_t)(sector_number & 0xFF));
    io_out_byte(ATA_SECTOR_MID, (uint8_t)((sector_number >> 8) & 0xFF));
    io_out_byte(ATA_SECTOR_HIGH, (uint8_t)((sector_number >> 16) & 0xFF));

    // Send the READ SECTORS command
    io_out_byte(ATA_CMD_STATUS, 0x20);                              // 0x20 is for reading

    // Wait until the drive is ready
    while (!(io_in_byte(ATA_CMD_STATUS) & 0x08));

    for (int i = 0; i < sector_count; i++)
    {
        for (int j = 0; j < 256; j++)
            ((uint16_t*)buffer)[j + i * 256] = io_in_word(ATA_DATA);
        wait_400ns();
        while (io_in_byte(ATA_CMD_STATUS) & 0x80);
    }

    return 0;
}

int ata_write(uint32_t sector_number, uint8_t sector_count, const void* buffer)
{
    if (!main_driver.is_present)
        return 1;
        
    io_out_byte(ATA_SELECT_DRIVE, (uint8_t)((sector_number >> 24) | 0xE0));   // 0xE0 is for LBA(sector_number) mode
    io_out_byte(ATA_SECTOR_COUNT, sector_count);                    // Output the amount of sectors to read

    // Send LBA(sector_number) bits 0-7, 8-15, and 16-23
    io_out_byte(ATA_SECTOR_LOW, (uint8_t)(sector_number & 0xFF));
    io_out_byte(ATA_SECTOR_MID, (uint8_t)((sector_number >> 8) & 0xFF));
    io_out_byte(ATA_SECTOR_HIGH, (uint8_t)((sector_number >> 16) & 0xFF));

    // Send the READ SECTORS command
    io_out_byte(ATA_CMD_STATUS, 0x30);                              // 0x30 is for reading

    // Wait until the drive is ready
    while (!(io_in_byte(ATA_CMD_STATUS) & 0x08));

    for (int i = 0; i < sector_count; i++)
    {
        for (int j = 0; j < 256; j++)
            io_out_word(ATA_DATA, ((uint16_t*)buffer)[j + i * 256]);
        wait_400ns();
        while (io_in_byte(ATA_CMD_STATUS) & 0x80);
    }

    return 0;
}


void wait_400ns()
{
    for (int i=0; i<4; i++)
        io_in_byte(ATA_CMD_STATUS);
}
