#include "fat.h"
#include "drivers/harddisk/ata/ata.h"
#include "drivers/vga/vga.h"
#include "memory/heap/heap.h"

#define IS_END_OF_CLUSTER_CHAIN(cluster) (cluster >= 0xFFF8 && cluster <= 0xFFFF)

// Global FAT16 filesystem structure
FAT16_FS fat16_fs;

uint16_t* fat_table; // will be heap allocated later
FAT16_DirEntry root_dir = {0};

// Static cluster operations
static int fat_read_data_cluster(uint32_t cluster_num, void *buffer);
static int fat_write_data_cluster(uint32_t cluster_num, const void *buffer);
static int get_ith_cluster(uint32_t starting_cluster, uint32_t i);

// FAT operations
static int fat_find_free();
static void fat_update_chain(int starting_fat, int new_fat_index);
static void fat_free_chain(int fat_index);

// Dir actions
static bool fat_find_dir_entry(const char *name, const FAT16_DirEntry *current_dir, FAT16_DirEntry *entry);
static bool fat_add_dir_entry(FAT16_DirEntry *parent_dir, const FAT16_DirEntry *new_entry);
static bool fat_update_dir_entry(const char *name, const FAT16_DirEntry *current_dir, const FAT16_DirEntry *new_entry);
static bool fat_remove_dir_entry(const char *name, const FAT16_DirEntry *current_dir, bool delete_chain);
static bool fat_allocate_space(uint32_t amount_of_clusters, const FAT16_DirEntry *entry);
static uint32_t fat_get_cluster_amount(const FAT16_DirEntry *entry);

static uint32_t fat_find_dir_entry_from_path(const char *path, FAT16_DirEntry *entry);

static bool check_file_name(const char *file_name);
static void get_parent_dir(const char *path, char *parent_dir);
static void get_base_name(const char *path, char *name);

bool fat_init()
{
    uint8_t boot_sector[512];
    
    // Read the boot sector (sector 0)
    if(ata_read(0, 1, (uint16_t *)&boot_sector))
    {
        vga_putstring("Failed to read boot sector.\n");
        return false;
    }

     // Verify the FAT16 signature
    uint16_t signature = *(uint16_t *)&boot_sector[510];
    if (signature != FAT16_SIGNATURE) {
        vga_printf("Invalid FAT16 signature: 0x%X\n", signature);
        return false;
    }

    // Parse boot sector
    fat16_fs.bytes_per_sector = *(uint16_t *)&boot_sector[11];
    fat16_fs.sectors_per_cluster = boot_sector[13];
    fat16_fs.reserved_sectors = *(uint16_t *)&boot_sector[14];
    fat16_fs.num_fats = boot_sector[16];
    fat16_fs.root_dir_entries = *(uint16_t *)&boot_sector[17];
    fat16_fs.total_sectors = *(uint16_t *)&boot_sector[19];
    fat16_fs.sectors_per_fat = *(uint16_t *)&boot_sector[22];

    // Calculate starting sectors
    fat16_fs.root_dir_start = fat16_fs.reserved_sectors + (fat16_fs.num_fats * fat16_fs.sectors_per_fat);
    fat16_fs.data_start = fat16_fs.root_dir_start + ((fat16_fs.root_dir_entries * 32) / fat16_fs.bytes_per_sector);

    // allocate fat
    fat_table = (uint16_t*)kmalloc(fat16_fs.sectors_per_fat * fat16_fs.bytes_per_sector);
    if (fat_table == NULL)
        return false;
    memset(fat_table, 0, fat16_fs.bytes_per_sector / sizeof(uint16_t) * fat16_fs.sectors_per_fat);

    // now read the whole fat table into ram
    for(int i = 0; i < fat16_fs.sectors_per_fat; i++)
    {
        if (ata_read(fat16_fs.reserved_sectors + i, 1, &fat_table[i * 256]) != 0) {
            vga_putstring("Failed to read FAT table.\n");
            return false;
        }
    }

    // Setup the root directory information
    strcpy(root_dir.name, "~");
    root_dir.start_cluster = 0;
    root_dir.attr |= FAT_ATTR_DIRECTORY;
    root_dir.file_size = fat16_fs.root_dir_entries * 32;

    vga_printf("FAT16 Initialized:\n");
    vga_printf("  Bytes Per Sector: %d\n", fat16_fs.bytes_per_sector);
    vga_printf("  Sectors Per Cluster: %d\n", fat16_fs.sectors_per_cluster);
    vga_printf("  Reserved Sectors: %d\n", fat16_fs.reserved_sectors);
    vga_printf("  Root Directory Start: %d\n", fat16_fs.root_dir_start);
    vga_printf("  Data Start: %d\n", fat16_fs.data_start);
    vga_printf("  Fat size(in sectors): %d\n", fat16_fs.num_fats * fat16_fs.sectors_per_fat);

    return true;
}


static int fat_find_free()
{
    // Calculate the total number of FAT entries
    int total_fat_entries = (fat16_fs.sectors_per_fat * fat16_fs.bytes_per_sector) / 2; // Each FAT16 entry is 2 bytes

    // Iterate through the FAT table to find a free entry
    for (int i = 0; i < total_fat_entries; i++) {
        if (fat_table[i] == FAT16_FREE_CLUSTER) {
            return i;  // Return the index of the free cluster
        }
    }

    return -1;  // No free cluster found
}


static void fat_update_chain(int starting_fat, int new_fat_index)
{
    uint8_t* buffer = (uint8_t*)kmalloc(fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster);
    memset(buffer, 0, fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster);
    int before_last = starting_fat;

    // find the before last entry to update
    while (!IS_END_OF_CLUSTER_CHAIN(fat_table[before_last]) && fat_table[before_last] != FAT16_FREE_CLUSTER)
    {
        before_last = fat_table[before_last];
        if (before_last < 0 || before_last >= fat16_fs.num_fats * fat16_fs.sectors_per_fat * fat16_fs.bytes_per_sector / 2) {
            // Prevent infinite loops if the FAT chain is corrupted
            vga_printf("Corrupted FAT chain detected!\n");
            return;
        }
    }
    // update this entry to point to a new entry
    fat_table[before_last] = new_fat_index;
    if (ata_write(
        fat16_fs.reserved_sectors + ((before_last * 2) / fat16_fs.bytes_per_sector),
        1,
        (uint16_t*)&fat_table[((before_last * 2) / fat16_fs.bytes_per_sector) * fat16_fs.bytes_per_sector / 2]
    ) != 0) {
        return;
    }
    
    
    // update this entry to point to a new entry
    fat_table[new_fat_index] = FAT16_CLUSTER_CHAIN_END;

    // Write the updated "new_fat_index" entry back to the FAT sectors on disk
    if(ata_write(
        fat16_fs.reserved_sectors + ((new_fat_index * 2) / fat16_fs.bytes_per_sector),
        1,
        (uint16_t*)&fat_table[((new_fat_index * 2) / fat16_fs.bytes_per_sector) * fat16_fs.bytes_per_sector / 2]
    ) != 0) {
        return;
    }

    fat_write_data_cluster(new_fat_index, buffer);
    kfree(buffer);
}

static void fat_free_chain(int fat_index)
{
    int current_index = fat_index;
    while (!IS_END_OF_CLUSTER_CHAIN(fat_table[current_index]) && fat_table[current_index] != FAT16_FREE_CLUSTER)
    {
        int next_index = fat_table[current_index];
        fat_table[current_index] = FAT16_FREE_CLUSTER;
        ata_write(
            fat16_fs.reserved_sectors + ((current_index * 2) / fat16_fs.bytes_per_sector),
            1,
            (uint16_t*)&fat_table[((current_index * 2) / fat16_fs.bytes_per_sector) * fat16_fs.bytes_per_sector / 2]
        );
        current_index = next_index;
    }
    
    fat_table[current_index] = FAT16_FREE_CLUSTER;
    ata_write(
        fat16_fs.reserved_sectors + ((current_index * 2) / fat16_fs.bytes_per_sector),
        1,
        (uint16_t*)&fat_table[((current_index * 2) / fat16_fs.bytes_per_sector) * fat16_fs.bytes_per_sector / 2]
    );
}


static int fat_read_data_cluster(uint32_t cluster_num, void *buffer)
{
    return ata_read(cluster_num * fat16_fs.sectors_per_cluster + fat16_fs.root_dir_start, fat16_fs.sectors_per_cluster, buffer);
}


static int fat_write_data_cluster(uint32_t cluster_num, const void *buffer)
{
    return ata_write(cluster_num * fat16_fs.sectors_per_cluster + fat16_fs.root_dir_start, fat16_fs.sectors_per_cluster, buffer);
}

static int get_ith_cluster(uint32_t starting_cluster, uint32_t i)
{
    while (i > 0)
    {
        if (IS_END_OF_CLUSTER_CHAIN(fat_table[starting_cluster]) || fat_table[starting_cluster] == FAT16_FREE_CLUSTER)
            return -1;

        starting_cluster = fat_table[starting_cluster];
        i--;
    }

    return starting_cluster;
}

static bool fat_find_dir_entry(const char *name, const FAT16_DirEntry *current_dir, FAT16_DirEntry *entry)
{
    int i = 0, cluster_num = current_dir->start_cluster;
    FAT16_DirEntry* dir = (FAT16_DirEntry*)kmalloc(fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster);
    if (fat_table == NULL)
        return false;
    memset(dir, 0, fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster);

    while ((cluster_num = get_ith_cluster(current_dir->start_cluster, i)) != -1)
    {   
        if (fat_read_data_cluster(cluster_num, dir))
        {
            kfree(dir);
            return false;
        }

        for (int dir_entry = 0; dir_entry < fat16_fs.bytes_per_sector / sizeof(FAT16_DirEntry) * fat16_fs.sectors_per_cluster; dir_entry++)
        {
            if (!strncmp(dir[dir_entry].name, name, FAT16_FILENAME_SIZE))
            {
                memcpy(entry, &dir[dir_entry], sizeof(FAT16_DirEntry));
                kfree(dir);
                return true;
            } 
        }

        i++;
    }
    kfree(dir);
    return false;
}

static bool fat_add_dir_entry(FAT16_DirEntry *parent_dir, const FAT16_DirEntry *new_entry)
{
    int i = 0, cluster_num = parent_dir->start_cluster;
    FAT16_DirEntry* dir = (FAT16_DirEntry*)kmalloc(fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster);
    if (fat_table == NULL)
        return false;
    memset(dir, 0, fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster);

    while ((cluster_num = get_ith_cluster(parent_dir->start_cluster, i)) != -1)
    {   
        if (fat_read_data_cluster(cluster_num, dir))
        {
            kfree(dir);
            return false;
        }

        for (int dir_entry = 0; dir_entry < fat16_fs.bytes_per_sector / sizeof(FAT16_DirEntry) * fat16_fs.sectors_per_cluster; dir_entry++)
        {
            if (!strncmp(dir[dir_entry].name, "", FAT16_FILENAME_SIZE))
            {
                memcpy(&dir[dir_entry], new_entry, sizeof(FAT16_DirEntry));

                if(fat_write_data_cluster(cluster_num, dir))
                {
                    kfree(dir);
                    return false;
                }
                
                kfree(dir);
                return true;
            } 
        }

        i++;
    }

    // If code got here it means the dir is full, means you need to extend it
    int new_fat_index = fat_find_free();

    if (new_fat_index == -1)
    {
        kfree(dir);
        return false;
    }

    fat_update_chain(parent_dir->start_cluster, new_fat_index);

    if(fat_read_data_cluster(new_fat_index, dir))
    {
        kfree(dir);
        return false;
    }

    memcpy(&dir[0], new_entry, sizeof(FAT16_DirEntry));

    if(fat_write_data_cluster(new_fat_index, dir))
    {
        kfree(dir);
        return false;
    }


    kfree(dir);
    return true;
}

static bool fat_update_dir_entry(const char *name, const FAT16_DirEntry *current_dir, const FAT16_DirEntry *new_entry)
{
    int i = 0, cluster_num = current_dir->start_cluster;
    FAT16_DirEntry* dir= (FAT16_DirEntry*)kmalloc(fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster);
    if (fat_table == NULL)
        return false;
    memset(dir, 0, fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster);

    while ((cluster_num = get_ith_cluster(current_dir->start_cluster, i)) != -1)
    {   
        if (fat_read_data_cluster(cluster_num, dir))
        {
            kfree(dir);
            return false;
        }

        for (int dir_entry = 0; dir_entry < fat16_fs.bytes_per_sector / sizeof(FAT16_DirEntry) * fat16_fs.sectors_per_cluster; dir_entry++)
        {
            if (!strcmp(dir[dir_entry].name, name))
            {
                memcpy(&dir[dir_entry], new_entry, sizeof(FAT16_DirEntry));
                if(fat_write_data_cluster(cluster_num, dir))
                {
                    kfree(dir);
                    return false;
                }
                kfree(dir);
                return true;
            } 
        }

        i++;
    }

    kfree(dir);
    return false;
}

static bool fat_remove_dir_entry(const char *name, const FAT16_DirEntry *current_dir, bool delete_chain)
{
    int i = 0, cluster_num = current_dir->start_cluster;
    FAT16_DirEntry* dir= (FAT16_DirEntry*)kmalloc(fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster);
    if (fat_table == NULL)
        return false;
    memset(dir, 0, fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster);

    while ((cluster_num = get_ith_cluster(current_dir->start_cluster, i)) != -1)
    {   
        if (fat_read_data_cluster(cluster_num, dir))
        {
            kfree(dir);
            return false;
        }

        for (int dir_entry = 0; dir_entry < fat16_fs.bytes_per_sector / sizeof(FAT16_DirEntry) * fat16_fs.sectors_per_cluster; dir_entry++)
        {
            if (!strncmp(dir[dir_entry].name, name, FAT16_FILENAME_SIZE))
            {
                if (delete_chain)
                    fat_free_chain(dir[dir_entry].start_cluster);

                memset(&dir[dir_entry], 0x00, sizeof(FAT16_DirEntry));
                fat_write_data_cluster(cluster_num, dir);
                kfree(dir);
                return true;
            } 
        }

        i++;
    }

    kfree(dir);
    return false;
}

static bool fat_allocate_space(uint32_t amount_of_clusters, const FAT16_DirEntry *entry)
{
    for (int i = 0; i < amount_of_clusters; i++)
    {
        int index = fat_find_free();

        if (index == -1)
            return false;

        fat_update_chain(entry->start_cluster, index);
    }

    return true;
}

static uint32_t fat_get_cluster_amount(const FAT16_DirEntry *entry)
{
    uint32_t count = 0;
    int starting_cluster = entry->start_cluster;

    while (!(IS_END_OF_CLUSTER_CHAIN(fat_table[starting_cluster]) || fat_table[starting_cluster] == FAT16_FREE_CLUSTER))
    {
        ++count;
        starting_cluster = fat_table[starting_cluster];
    }

    if (IS_END_OF_CLUSTER_CHAIN(fat_table[starting_cluster]))
        ++count;

    return count;
}

static uint32_t fat_find_dir_entry_from_path(const char *path, FAT16_DirEntry *entry)
{
    FAT16_DirEntry curr_dir_entry = {0};
    memcpy(&curr_dir_entry, &root_dir, sizeof(FAT16_DirEntry));  
    char sub_name[FAT16_FILENAME_SIZE + 1] = {0};
    int index = 0, path_len = strlen(path);

    if (!strncmp(path, "~/", sizeof("~/")) || !strncmp(path, "/", sizeof("/")) || !strncmp(path, "./", sizeof("./")))
    {
        memcpy(entry, &root_dir, sizeof(FAT16_DirEntry));
        return SUCCESS;
    }
    
    while (true)
    {
        int start_index = index;
        // Iterate over the index until found a '/' or the length is invalid
        while(index - start_index <= FAT16_FILENAME_SIZE && path[index] != '/' && index < path_len)
            ++index;

        // Check if length is invalid
        if (index - start_index > FAT16_FILENAME_SIZE)
            return DIR_NAME_EXCEEDS_LENGTH;

        memset(sub_name, 0, FAT16_FILENAME_SIZE);
        strncpy(sub_name, path + start_index, index - start_index);
        
        if (start_index == 0)
        {
            // End of the path
            if (strncmp(sub_name, "", FAT16_FILENAME_SIZE) != 0 && strncmp(sub_name, "~", FAT16_FILENAME_SIZE) != 0)
                return DIR_NOT_FOUND;

            ++index;
            continue;
        }

        // Check if it's the last part of the path
        if (index >= path_len - 1)
        {
            if(!fat_find_dir_entry(sub_name, &curr_dir_entry, &curr_dir_entry))
            {   
                // If the last index isn't out of bounds it means its a '/', which represents a directory
                if (index == path_len - 1) 
                    return DIR_NOT_FOUND;
                return FILE_NOT_FOUND;
            }

            memcpy(entry, &curr_dir_entry, sizeof(FAT16_DirEntry));
            return SUCCESS;
        }
        if (!(curr_dir_entry.attr & FAT_ATTR_DIRECTORY))
            return DIR_NOT_FOUND;
        if(!fat_find_dir_entry(sub_name, &curr_dir_entry, &curr_dir_entry))
            return DIR_NOT_FOUND;

        index++;
    }
}

static bool check_file_name(const char *file_name)
{
    // Check for legal characters (letters, digits, '-', '_', '.')
    for (const char *p = file_name; *p; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '-' || *p == '_' || *p == '.')) {
            return false;
        }
    }

    if (!strcmp(file_name, "") || !strcmp(file_name, ".") || !strcmp(file_name, "..") || !strcmp(file_name, "~"))
        return false;

    return true;
}

static void get_parent_dir(const char *path, char *parent_dir)
{
    char path_copy[1024];
    strcpy(path_copy, path);

    char *last_slash = strrchr(path_copy, '/');
    if (last_slash) {
        *(last_slash) = '\0';
    }

    char *directory = path_copy;
    
    if (strlen(directory) == 0)
    {
        strcpy(parent_dir, "/");
        return;
    }

    strcpy(parent_dir, directory);
}

static void get_base_name(const char *path, char *name)
{
    char path_copy[1024];
    strcpy(path_copy, path);

    char *last_slash = strrchr(path_copy, '/');
    if (last_slash) {
        *last_slash = '\0';
    }

    char *base_name = last_slash ? last_slash + 1 : path_copy;

    strcpy(name, base_name);
}

static uint32_t fat_create(const char *path, bool is_directory)
{
    FAT16_DirEntry parent_dir;
    FAT16_DirEntry dir;
    FAT16_DirEntry dot_entries;
    int err = 0;
    char directory[1024] = {0};
    char dir_name[12] = {0};

    get_base_name(path, dir_name);
    get_parent_dir(path, directory);

    if(!check_file_name(dir_name))
        return INVALID_FILE_NAME;

    err = fat_find_dir_entry_from_path(directory, &parent_dir);

    if (err)
        return err;

    // check if the parent dir is a directory and not a file
    if (!(parent_dir.attr & FAT_ATTR_DIRECTORY))
        return DIR_NOT_FOUND;

    if(fat_find_dir_entry(dir_name, &parent_dir, &dir))
        return FILE_ALREADY_EXISTS;

    memset(&dir, 0, sizeof(FAT16_DirEntry));
    dir.start_cluster = fat_find_free();
    if (dir.start_cluster == -1)
        return CANT_ALLOCATE_SPACE;

    fat_update_chain(dir.start_cluster , dir.start_cluster);
    strncpy(dir.name, dir_name, FAT16_FILENAME_SIZE);

    if (is_directory)
    {
        dir.attr |= FAT_ATTR_DIRECTORY; // mark as directory
        dir.file_size = 2; // . and .. entries
    }

    if(!fat_add_dir_entry(&parent_dir, &dir))
    {
        fat_free_chain(dir.start_cluster);
        return GENERAL_ERROR;
    }

    // create the . and .. directories
    if (is_directory)
    {
        memset(&dot_entries, 0, sizeof(FAT16_DirEntry));
        dot_entries.start_cluster = dir.start_cluster;
        dot_entries.attr |= FAT_ATTR_DIRECTORY;
        strcpy(dot_entries.name, ".");
        if(!fat_add_dir_entry(&dir, &dot_entries))
        {
            fat_free_chain(dir.start_cluster);
            return GENERAL_ERROR;
        }

        memset(&dot_entries, 0, sizeof(FAT16_DirEntry));
        dot_entries.start_cluster = parent_dir.start_cluster;
        dot_entries.attr |= FAT_ATTR_DIRECTORY;
        strcpy(dot_entries.name, "..");
        if(!fat_add_dir_entry(&dir, &dot_entries))
        {
            fat_free_chain(dir.start_cluster);
            return GENERAL_ERROR;
        }
    }

    // Update the parent dir count
    parent_dir.file_size++;

    char parent_parent[1024] = {0};
    FAT16_DirEntry parent_parent_dir;
    get_parent_dir(directory, parent_parent);
    err = fat_find_dir_entry_from_path(parent_parent, &parent_parent_dir);

    if (err)
        return err;

    fat_update_dir_entry(parent_dir.name, &parent_parent_dir, &parent_dir);

    return SUCCESS;
}

static uint32_t fat_delete(const char *path, bool is_directory)
{
    FAT16_DirEntry parent_dir;
    FAT16_DirEntry parent_parent_dir;
    FAT16_DirEntry dir;
    int err = 0;
    char parent_dir_name[1024] = {0};
    char parent_parent_dir_name[1024] = {0};
    char dir_name[12] = {0};

    get_base_name(path, dir_name);
    get_parent_dir(path, parent_dir_name);
    get_parent_dir(parent_dir_name, parent_parent_dir_name);

    err = fat_find_dir_entry_from_path(path, &dir);
    if (err)
        return err;

    if (is_directory)
    {
        if (!(dir.attr && FAT_ATTR_DIRECTORY))
            return DIR_NOT_FOUND;
        
        if (dir.file_size != 2)
            return DIRECTORY_ISNT_EMPTY;
    }
    else if (dir.attr & FAT_ATTR_DIRECTORY)
    {
        return IS_DIR;
    }

    err = fat_find_dir_entry_from_path(parent_dir_name, &parent_dir);
    if (err)
        return err;

    err = fat_find_dir_entry_from_path(parent_parent_dir_name, &parent_parent_dir);
    if (err)
        return err;

    fat_remove_dir_entry(dir_name, &parent_dir, true);

    parent_dir.file_size--;
    fat_update_dir_entry(parent_dir.name, &parent_parent_dir, &parent_dir);

    return SUCCESS;
}

uint32_t fat_create_file(const char *path)
{
    fat_create(path, false);
}

uint32_t fat_create_directory(const char *path)
{
    fat_create(path, true);
}

uint32_t fat_rename(const char *path, const char *new_name)
{
    FAT16_DirEntry file;
    FAT16_DirEntry parent_dir;
    char directory[1024] = {0};
    char previous_name[12] = {0};
    int err = fat_find_dir_entry_from_path(path, &file);

    if (err)
        return err;

    get_parent_dir(path, directory);

    err = fat_find_dir_entry_from_path(directory, &parent_dir);

    if (err)
        return err;

    // check if the parent dir is a directory and not a file
    if (!(parent_dir.attr & FAT_ATTR_DIRECTORY))
        return DIR_NOT_FOUND;

    if (strlen(new_name) > FAT16_FILENAME_SIZE)
        return FILE_NAME_EXCEEDS_LENGTH;
    if (!check_file_name(new_name))
        return INVALID_FILE_NAME;

    strncpy(previous_name, file.name, FAT16_FILENAME_SIZE);
    memset(file.name, 0, FAT16_FILENAME_SIZE);
    strncpy(file.name, new_name, FAT16_FILENAME_SIZE);

    if (fat_find_dir_entry(file.name, &parent_dir, &file))
        return FILE_ALREADY_EXISTS;

    if(!fat_update_dir_entry(previous_name, &parent_dir, &file))
        return GENERAL_ERROR;
    
    return SUCCESS;
}

int fat_get_file_data(const char *path, FileData *fileData)
{
    FAT16_DirEntry file;
    FileData data = {0};  // Initialize all to zero first
    int err = fat_find_dir_entry_from_path(path, &file);

    if (err)
        return -ENOENT;

    if (file.attr & FAT_ATTR_DIRECTORY)
        return -EISDIR;

    // Copy the complete file entry
    memcpy(&data.file_entry, &file, sizeof(FAT16_DirEntry));
    
    // Get parent directory
    char parent_path[1024] = {0};
    get_parent_dir(path, parent_path);
    err = fat_find_dir_entry_from_path(parent_path, &data.parent_entry);


    if (err)
        return -1;

    memcpy(fileData, &data, sizeof(FileData));

    return 0;
}

int fat_get_dir_data(const char *path, FileData *fileData)
{
    FAT16_DirEntry dir;
    FileData data = {0};  // Initialize all to zero first
    int err = fat_find_dir_entry_from_path(path, &dir);

    if (err)
        return -1;

    if (!(dir.attr & FAT_ATTR_DIRECTORY))
        return -1;

    // Copy the complete directory entry
    memcpy(&data.file_entry, &dir, sizeof(FAT16_DirEntry));
    
    // Get parent directory
    char parent_path[1024] = {0};
    get_parent_dir(path, parent_path);
    err = fat_find_dir_entry_from_path(parent_path, &data.parent_entry);
    if (err)
        return -1;

    memcpy(fileData, &data, sizeof(FileData));

    return 0;
}

uint32_t fat_delete_file(const char *path)
{
    return fat_delete(path, false);
}

uint32_t fat_delete_dir(const char *path)
{
    return fat_delete(path, true);
}

// Read data from a file
// Returns number of bytes read, or negative value on error
int32_t fat_read(FAT16_DirEntry* file, uint32_t offset, uint32_t size, void* buffer) {
    // Check if file is actually a directory
    if (file->attr & FAT_ATTR_DIRECTORY) {
        return -1;
    }

    // Check if offset is beyond file size
    if (offset >= file->file_size) {
        return 0;  // Reading past end of file
    }

    // Adjust size if it would read past end of file
    if (offset + size > file->file_size) {
        size = file->file_size - offset;
    }

    // Calculate starting cluster and offset within cluster
    uint32_t bytes_per_cluster = fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster;
    uint32_t start_cluster_index = offset / bytes_per_cluster;
    uint32_t cluster_offset = offset % bytes_per_cluster;

    // Find the starting cluster by traversing the chain
    uint32_t current_cluster = file->start_cluster;
    for (uint32_t i = 0; i < start_cluster_index; i++) {
        if (current_cluster >= FAT16_CLUSTER_CHAIN_END || current_cluster == 0) {
            return -1; // Reached end of chain prematurely
        }
        current_cluster = fat_table[current_cluster];
    }

    uint32_t bytes_read = 0;
    uint8_t* buf_ptr = (uint8_t*)buffer;
    uint8_t* cluster_buffer = (uint8_t*)kmalloc(fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster);
    if (cluster_buffer == NULL)
        return GENERAL_ERROR;
    memset(cluster_buffer, 0, fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster);

    while (bytes_read < size && current_cluster < FAT16_CLUSTER_CHAIN_END && current_cluster != 0) {
        // Read entire cluster
        if (fat_read_data_cluster(current_cluster, cluster_buffer)) {
            kfree(cluster_buffer);
            return -1;
        }

        // Calculate how many bytes to copy from this cluster
        uint32_t bytes_to_copy = bytes_per_cluster - cluster_offset;
        if (bytes_to_copy > (size - bytes_read)) {
            bytes_to_copy = size - bytes_read;
        }

        // Copy data from cluster to output buffer
        memcpy(buf_ptr + bytes_read, 
               cluster_buffer + cluster_offset, 
               bytes_to_copy);

        bytes_read += bytes_to_copy;
        cluster_offset = 0; // Reset offset for subsequent clusters
        
        // Move to next cluster
        if (bytes_read < size) {
            current_cluster = fat_table[current_cluster];
        }
    }

    kfree(cluster_buffer);

    return bytes_read;
}

// Write data to a file
// Returns number of bytes written, or negative value on error
int32_t fat_write(FAT16_DirEntry* file, FAT16_DirEntry* parent_dir, uint32_t offset, uint32_t size, const void* buffer) 
{
    // Check if file is actually a directory
    if (file->attr & FAT_ATTR_DIRECTORY) 
    {
        return -1;
    }

    uint32_t bytes_per_cluster = fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster;
    uint32_t start_cluster_index = offset / bytes_per_cluster;
    uint32_t cluster_offset = offset % bytes_per_cluster;
    
    // Ensure the file has enough space
    uint32_t new_size = offset + size;
    if (new_size > file->file_size) 
    {
        FileData fileData;
        memcpy(&fileData.file_entry, file, sizeof(FAT16_DirEntry));
        memcpy(&fileData.parent_entry, parent_dir, sizeof(FAT16_DirEntry));
        if (fat_truncate(&fileData, new_size) != 0) 
        {
            return -1;
        }
        memcpy(file, &fileData.file_entry, sizeof(FAT16_DirEntry));
    }

    // Traverse to the starting cluster
    uint32_t current_cluster = file->start_cluster;
    for (uint32_t i = 0; i < start_cluster_index; i++) 
    {
        if (current_cluster >= FAT16_CLUSTER_CHAIN_END) 
        {
            return -1;
        }
        current_cluster = fat_table[current_cluster];
    }

    uint32_t bytes_written = 0;
    const uint8_t* buf_ptr = (const uint8_t*)buffer;
    uint8_t* cluster_buffer = (uint8_t*)kmalloc(fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster);
    if (cluster_buffer == NULL)
        return GENERAL_ERROR;
    memset(cluster_buffer, 0, fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster);

    while (bytes_written < size) 
    {
        // Read existing cluster data if we're not writing a complete cluster
        if (cluster_offset > 0 || (size - bytes_written) < bytes_per_cluster) 
        {
            if (fat_read_data_cluster(current_cluster, cluster_buffer)) 
            {
                kfree(cluster_buffer);
                return -1;
            }
        }

        // Calculate how many bytes to write to this cluster
        uint32_t bytes_to_write = bytes_per_cluster - cluster_offset;
        if (bytes_to_write > (size - bytes_written)) 
        {
            bytes_to_write = size - bytes_written;
        }

        // Copy data from input buffer to cluster buffer
        memcpy(cluster_buffer + cluster_offset,
               buf_ptr + bytes_written,
               bytes_to_write);

        // Write cluster back to disk
        if (fat_write_data_cluster(current_cluster, cluster_buffer)) 
        {
            kfree(cluster_buffer);
            return -1;
        }

        bytes_written += bytes_to_write;
        cluster_offset = 0; // Reset offset for subsequent clusters

        // Move to next cluster
        if (bytes_written < size) 
        {
            current_cluster = fat_table[current_cluster];
        }
    }

    kfree(cluster_buffer);

    return bytes_written;
}

int fat_truncate(FileData* file, uint32_t size)
{
    // Check if file is actually a directory
    if (file->file_entry.attr & FAT_ATTR_DIRECTORY) 
    {
        return -1;
    }

    // if new size is smaller then the current size, free the extra clusters
    if (size < file->file_entry.file_size) 
    {
        int curr_cluster_amount = fat_get_cluster_amount(&file->file_entry);
        int new_cluster_amount = size / (fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster) + 1;

        int cluster = file->file_entry.start_cluster;

        while (new_cluster_amount < curr_cluster_amount)
        {
            int next_cluster = fat_table[cluster];
            fat_table[cluster] = FAT16_FREE_CLUSTER;
            cluster = next_cluster;
            curr_cluster_amount--;
        }
        fat_table[cluster] = FAT16_CLUSTER_CHAIN_END;
    }
    else if(size > file->file_entry.file_size)
    {
        // if new size is bigger then the current size, allocate new clusters
        int new_cluster_amount = size / (fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster) + 1;
        int curr_cluster_amount = fat_get_cluster_amount(&file->file_entry);

        if (!fat_allocate_space(new_cluster_amount - curr_cluster_amount, &file->file_entry)) 
        {
            return -1;
        }
        char* buf = kmalloc(size - file->file_entry.file_size);
        if (buf == NULL)
            return -1;
        memset(buf, 0, size - file->file_entry.file_size);
        file->file_entry.file_size = size;
        fat_write(&file->file_entry, &file->parent_entry, file->file_entry.file_size, size - file->file_entry.file_size, buf);
        kfree(buf);
    }
    else 
    {
        // No need to change anything
        return 0;
    }

    // Update file size in directory entry
    file->file_entry.file_size = size;
    if (!fat_update_dir_entry(file->file_entry.name, &file->parent_entry, &file->file_entry)) 
    {
        return -1;
    }
    return 0;
}

int fat_get_dir_entry(FAT16_DirEntry *dir, int n, FAT16_DirEntry *entry)
{
    int count = 0, cluster_num = dir->start_cluster, i = 0;
    if (n >= dir->file_size)
        return FILE_NOT_FOUND;

    FAT16_DirEntry* dir_buff = (FAT16_DirEntry*)kmalloc(fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster);
    if (fat_table == NULL)
        return false;
    memset(dir_buff, 0, fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster);

    while ((cluster_num = get_ith_cluster(dir->start_cluster, i)) != -1)
    {   
        if (fat_read_data_cluster(cluster_num, dir_buff))
        {
            kfree(dir_buff);
            return CANT_ALLOCATE_SPACE;
        }

        for (int dir_entry = 0; dir_entry < fat16_fs.bytes_per_sector / sizeof(FAT16_DirEntry) * fat16_fs.sectors_per_cluster; dir_entry++)
        {
            if (count == n)
            {
                memcpy(entry, &dir_buff[dir_entry], sizeof(FAT16_DirEntry));
                kfree(dir_buff);
                return 0;
            }
            if (strncmp(dir_buff[dir_entry].name, "", FAT16_FILENAME_SIZE))
                count++;
        }

        i++;
    }

    return FILE_NOT_FOUND;
}   