#include "fat_loader.h"

#define IS_END_OF_CLUSTER_CHAIN(cluster) (cluster >= 0xFFF8 && cluster <= 0xFFFF)

// Global FAT16 filesystem structure
FAT16_FS fat16_fs;

uint16_t* fat_table; // will be heap allocated later
FAT16_DirEntry root_dir = {0};

#define SECTOR_SIZE 512

static FILE* ata_file = NULL;

// Cluster operations
static int fat_read_data_cluster(uint32_t cluster_num, void *buffer);
static int fat_write_data_cluster(uint32_t cluster_num, const void *buffer);
static int get_ith_cluster(uint32_t starting_cluster, uint32_t i);
// FAT operations
static int fat_find_free();
static void fat_update_chain(int starting_fat, int new_fat_index);
static void fat_free_chain(int fat_index);

// Dir actions
static bool fat_find_dir_entry(const char *name, const FAT16_DirEntry *current_dir, FAT16_DirEntry *entry);
static bool fat_add_dir_entry(FAT16_DirEntry *mother_dir, const FAT16_DirEntry *new_entry);
static bool fat_update_dir_entry(const char *name, const FAT16_DirEntry *current_dir, const FAT16_DirEntry *new_entry);
static bool fat_remove_dir_entry(const char *name, const FAT16_DirEntry *current_dir, bool delete_chain);
static bool fat_allocate_space(uint32_t amount_of_clusters, const FAT16_DirEntry *entry);
static uint32_t fat_get_cluster_amount(const FAT16_DirEntry *entry);

static uint32_t fat_find_dir_entry_from_path(const char *path, FAT16_DirEntry *entry);

static bool check_file_name(const char *file_name);
static void get_mother_dir(const char *path, char *mother_dir);
static void get_base_name(const char *path, char *name);

// file system generic functions
static uint32_t fat_create(const char *path, bool is_directory);
static uint32_t fat_delete(const char *path, bool is_directory);

int main(int argc, char** argv)
{
    int err = 0;
    if (argc < 4)
    {
        printf("Format: ./fat_loader <disk image> <path to load> <local file to load>\n");
        exit(1);
    }

    ata_init(argv[1]);
    fat_init();
    if ((err = fat_create_file(argv[2])))
    {
        printf("Can't create file: %d\n", err);
        ata_cleanup();
        exit(1);
    }
    FileData d = fat_get_file_data(argv[2]);

     // Read the local file into a buffer
    FILE* local_file = fopen(argv[3], "rb");
    if (!local_file) {
        printf("Could not open local file %s\n", argv[3]);
        ata_cleanup();
        exit(1);
    }

    // Determine the size of the local file
    fseek(local_file, 0, SEEK_END);
    long file_size = ftell(local_file);
    rewind(local_file);

    uint8_t* buffer = malloc(file_size);
    if (!buffer) {
        printf("Could not allocate memory for file buffer\n");
        fclose(local_file);
        ata_cleanup();
        exit(1);
    }

    if (fread(buffer, 1, file_size, local_file) != file_size) {
        printf("Could not read file into buffer\n");
        free(buffer);
        fclose(local_file);
        ata_cleanup();
        exit(1);
    }
    fclose(local_file);

    fat_write(&d.file_entry, &d.parent_entry, 0, file_size, buffer);

    free(buffer);

    ata_cleanup();
}


// Initializes the ATA emulation with a file
void ata_init(const char* filename) {
    ata_file = fopen(filename, "r+b"); // Open for read and write in binary mode
    if (!ata_file) {
        fprintf(stderr, "Error: Could not open file %s\n", filename);
    }
}

int ata_read(uint32_t sector_number, uint8_t sector_count, uint16_t* buffer) {
    if (!ata_file) {
        fprintf(stderr, "Error: ATA file not initialized\n");
        return 1;
    }

    // Calculate the offset in the file
    uint64_t offset = (uint64_t)sector_number * SECTOR_SIZE;

    // Seek to the correct position in the file
    if (fseek(ata_file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "Error: Could not seek to sector %u\n", sector_number);
        return 1;
    }

    // Read the sectors into the buffer
    size_t bytes_to_read = sector_count * SECTOR_SIZE;
    if (fread(buffer, 1, bytes_to_read, ata_file) != bytes_to_read) {
        fprintf(stderr, "Error: Could not read %u sectors from file\n", sector_count);
        return 1;
    }

    return 0;
}

int ata_write(uint32_t sector_number, uint8_t sector_count, const uint16_t* buffer) {
    if (!ata_file) {
        fprintf(stderr, "Error: ATA file not initialized\n");
        return 1;
    }

    // Calculate the offset in the file
    uint64_t offset = (uint64_t)sector_number * SECTOR_SIZE;

    // Seek to the correct position in the file
    if (fseek(ata_file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "Error: Could not seek to sector %u\n", sector_number);
        return 1;
    }

    // Write the buffer to the file
    size_t bytes_to_write = sector_count * SECTOR_SIZE;
    if (fwrite(buffer, 1, bytes_to_write, ata_file) != bytes_to_write) {
        fprintf(stderr, "Error: Could not write %u sectors to file\n", sector_count);
        return 1;
    }

    // Flush to ensure data is written to disk
    fflush(ata_file);

    return 0;
}

// Cleanup function to close the ATA file
void ata_cleanup() {
    if (ata_file) {
        fclose(ata_file);
        ata_file = NULL;
    }
}


bool fat_init()
{
    uint8_t boot_sector[512];
    
    // Read the boot sector (sector 0)
    if(ata_read(0, 1, (uint16_t *)&boot_sector))
    {
        puts("Failed to read boot sector.\n");
        return false;
    }

     // Verify the FAT16 signature
    uint16_t signature = *(uint16_t *)&boot_sector[510];
    if (signature != FAT16_SIGNATURE) {
        printf("Invalid FAT16 signature: 0x%X\n", signature);
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
    fat_table = (uint16_t*)malloc(256 * 256);
    if (fat_table == NULL)
        return false;
    memset(fat_table, 0, fat16_fs.bytes_per_sector / sizeof(uint16_t) * fat16_fs.sectors_per_fat);

    // now read the whole fat table into ram
    for(int i = 0; i < fat16_fs.sectors_per_fat; i++)
    {
        if (ata_read(fat16_fs.reserved_sectors + i, 1, &fat_table[i * 256]) != 0) {
            puts("Failed to read FAT table.\n");
            return false;
        }
    }

    // Setup the root directory information
    strcpy(root_dir.name, "~");
    root_dir.start_cluster = 0;
    root_dir.attr |= FAT_ATTR_DIRECTORY;
    root_dir.file_size = fat16_fs.root_dir_entries * 32;

    printf("FAT16 Initialized:\n");
    printf("  Bytes Per Sector: %d\n", fat16_fs.bytes_per_sector);
    printf("  Sectors Per Cluster: %d\n", fat16_fs.sectors_per_cluster);
    printf("  Reserved Sectors: %d\n", fat16_fs.reserved_sectors);
    printf("  Root Directory Start: %d\n", fat16_fs.root_dir_start);
    printf("  Data Start: %d\n", fat16_fs.data_start);
    printf("  Fat size(in sectors): %d\n", fat16_fs.num_fats * fat16_fs.sectors_per_fat);

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
    uint8_t* buffer = (uint8_t*)malloc(sizeof(uint8_t) * fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster);
    memset(buffer, 0, sizeof(uint8_t) * fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster);
    int before_last = starting_fat;

    // find the before last entry to update
    while (!IS_END_OF_CLUSTER_CHAIN(fat_table[before_last]) && fat_table[before_last] != FAT16_FREE_CLUSTER)
    {
        before_last = fat_table[before_last];
        if (before_last < 0 || before_last >= fat16_fs.num_fats * fat16_fs.sectors_per_fat * fat16_fs.bytes_per_sector / 2) {
            // Prevent infinite loops if the FAT chain is corrupted
            printf("Corrupted FAT chain detected!\n");
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
    free(buffer);
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
    FAT16_DirEntry* dir= (FAT16_DirEntry*)malloc(fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster);
    if (fat_table == NULL)
        return false;
    memset(dir, 0, fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster);

    while ((cluster_num = get_ith_cluster(current_dir->start_cluster, i)) != -1)
    {   
        if (fat_read_data_cluster(cluster_num, dir))
        {
            free(dir);
            return false;
        }

        for (int dir_entry = 0; dir_entry < fat16_fs.bytes_per_sector / sizeof(FAT16_DirEntry) * fat16_fs.sectors_per_cluster; dir_entry++)
        {
            if (!strncmp(dir[dir_entry].name, name, FAT16_FILENAME_SIZE))
            {
                memcpy(entry, &dir[dir_entry], sizeof(FAT16_DirEntry));
                free(dir);
                return true;
            } 
        }

        i++;
    }
    free(dir);
    return false;
}

static bool fat_add_dir_entry(FAT16_DirEntry *parent_dir, const FAT16_DirEntry *new_entry)
{
    int i = 0, cluster_num = parent_dir->start_cluster;
    FAT16_DirEntry* dir= (FAT16_DirEntry*)malloc(fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster);
    if (fat_table == NULL)
        return false;
    memset(dir, 0, fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster);

    while ((cluster_num = get_ith_cluster(parent_dir->start_cluster, i)) != -1)
    {   
        if (fat_read_data_cluster(cluster_num, dir))
        {
            free(dir);
            return false;
        }

        for (int dir_entry = 0; dir_entry < fat16_fs.bytes_per_sector / sizeof(FAT16_DirEntry) * fat16_fs.sectors_per_cluster; dir_entry++)
        {
            if (!strncmp(dir[dir_entry].name, "", FAT16_FILENAME_SIZE))
            {
                memcpy(&dir[dir_entry], new_entry, sizeof(FAT16_DirEntry));

                if(fat_write_data_cluster(cluster_num, dir))
                {
                    free(dir);
                    return false;
                }
                
                free(dir);
                return true;
            } 
        }

        i++;
    }

    // If code got here it means the dir is full, means you need to extend it
    int new_fat_index = fat_find_free();

    if (new_fat_index == -1)
    {
        free(dir);
        return false;
    }

    fat_update_chain(parent_dir->start_cluster, new_fat_index);

    if(fat_read_data_cluster(new_fat_index, dir))
    {
        free(dir);
        return false;
    }

    memcpy(&dir[0], new_entry, sizeof(FAT16_DirEntry));

    if(fat_write_data_cluster(new_fat_index, dir))
    {
        free(dir);
        return false;
    }


    free(dir);
    return true;
}

static bool fat_update_dir_entry(const char *name, const FAT16_DirEntry *current_dir, const FAT16_DirEntry *new_entry)
{
    int i = 0, cluster_num = current_dir->start_cluster;
    FAT16_DirEntry* dir= (FAT16_DirEntry*)malloc(fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster);
    if (fat_table == NULL)
        return false;
    memset(dir, 0, fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster);

    while ((cluster_num = get_ith_cluster(current_dir->start_cluster, i)) != -1)
    {   
        if (fat_read_data_cluster(cluster_num, dir))
        {
            free(dir);
            return false;
        }

        for (int dir_entry = 0; dir_entry < fat16_fs.bytes_per_sector / sizeof(FAT16_DirEntry) * fat16_fs.sectors_per_cluster; dir_entry++)
        {
            if (!strcmp(dir[dir_entry].name, name))
            {
                memcpy(&dir[dir_entry], new_entry, sizeof(FAT16_DirEntry));
                if(fat_write_data_cluster(cluster_num, dir))
                {
                    free(dir);
                    return false;
                }
                free(dir);
                return true;
            } 
        }

        i++;
    }

    free(dir);
    return false;
}

static bool fat_remove_dir_entry(const char *name, const FAT16_DirEntry *current_dir, bool delete_chain)
{
    int i = 0, cluster_num = current_dir->start_cluster;
    FAT16_DirEntry* dir= (FAT16_DirEntry*)malloc(fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster);
    if (fat_table == NULL)
        return false;
    memset(dir, 0, fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster);

    while ((cluster_num = get_ith_cluster(current_dir->start_cluster, i)) != -1)
    {   
        if (fat_read_data_cluster(cluster_num, dir))
        {
            free(dir);
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
                free(dir);
                return true;
            } 
        }

        i++;
    }

    free(dir);
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
        dir.attr |= FAT_ATTR_DIRECTORY; // mark as directory

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

    parent_dir.file_size++;

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
        
        if (dir.file_size != 0)
            return DIRECTORY_ISNT_EMPTY;
    }
    else if (dir.attr & FAT_ATTR_DIRECTORY)
    {
        return FILE_NOT_FOUND;
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

FileData fat_get_file_data(const char *path)
{
    FAT16_DirEntry file;
    FileData data = {0};  // Initialize all to zero first
    int err = fat_find_dir_entry_from_path(path, &file);

    if (err)
        return data;

    if (file.attr & FAT_ATTR_DIRECTORY)
        return data;

    // Copy the complete file entry
    memcpy(&data.file_entry, &file, sizeof(FAT16_DirEntry));
    
    // Get parent directory
    char parent_path[1024] = {0};
    get_parent_dir(path, parent_path);
    err = fat_find_dir_entry_from_path(parent_path, &data.parent_entry);


    if (err)
        return data;

    return data;
}

FileData fat_get_dir_data(const char *path)
{
    FAT16_DirEntry dir;
    FileData data = {0};  // Initialize all to zero first
    int err = fat_find_dir_entry_from_path(path, &dir);

    if (err)
        return data;

    if (!(dir.attr & FAT_ATTR_DIRECTORY))
        return data;

    // Copy the complete directory entry
    memcpy(&data.file_entry, &dir, sizeof(FAT16_DirEntry));
    
    // Get parent directory
    char parent_path[1024] = {0};
    get_parent_dir(path, parent_path);
    err = fat_find_dir_entry_from_path(parent_path, &data.parent_entry);
    if (err)
        return data;

    return data;
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
    uint8_t* cluster_buffer = (uint8_t*)malloc(fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster);
    if (cluster_buffer == NULL)
        return GENERAL_ERROR;
    memset(cluster_buffer, 0, fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster);

    while (bytes_read < size && current_cluster < FAT16_CLUSTER_CHAIN_END && current_cluster != 0) {
        // Read entire cluster
        if (fat_read_data_cluster(current_cluster, cluster_buffer)) {
            free(cluster_buffer);
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

    free(cluster_buffer);

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
    
    // Find or allocate starting cluster
    uint32_t current_cluster = file->start_cluster;
    if (current_cluster == 0) 
    {
        // New file, allocate first cluster
        current_cluster = fat_find_free();
        if (current_cluster == -1) 
        {
            return -1;
        }
        file->start_cluster = current_cluster;
        fat_table[current_cluster] = FAT16_CLUSTER_CHAIN_END; // Mark as end of chain
        
        // Write FAT table updates to disk
        for (int fat = 0; fat < fat16_fs.num_fats; fat++) {
            uint32_t fat_sector = fat16_fs.reserved_sectors + 
                                 fat * fat16_fs.sectors_per_fat + 
                                 (current_cluster * 2) / fat16_fs.bytes_per_sector;
            uint32_t sector_offset = (current_cluster * 2) % fat16_fs.bytes_per_sector;
            
            uint8_t fat_sector_data[512];
            if (ata_read(fat_sector, 1, fat_sector_data)) {
                return -1;
            }
            
            // Update the entry in the sector
            *(uint16_t*)(fat_sector_data + sector_offset) = FAT16_CLUSTER_CHAIN_END;
            
            if (ata_write(fat_sector, 1, fat_sector_data)) {
                return -1;
            }
        }
    }

    // Traverse to the starting cluster
    for (uint32_t i = 0; i < start_cluster_index; i++) 
    {
        if (current_cluster >= FAT16_CLUSTER_CHAIN_END) {
            // Need to extend the chain
            uint32_t new_cluster = fat_find_free();
            if (new_cluster == -1) {
                return -1;
            }
            
            // Update FAT chain
            fat_table[current_cluster] = new_cluster;
            fat_table[new_cluster] = FAT16_CLUSTER_CHAIN_END;
            
            // Write FAT table updates to disk
            for (int fat = 0; fat < fat16_fs.num_fats; fat++) {
                // Write current cluster's new value
                uint32_t fat_sector = fat16_fs.reserved_sectors + 
                                    fat * fat16_fs.sectors_per_fat + 
                                    (current_cluster * 2) / fat16_fs.bytes_per_sector;
                uint32_t sector_offset = (current_cluster * 2) % fat16_fs.bytes_per_sector;
                
                uint8_t fat_sector_data[512];
                if (ata_read(fat_sector, 1, fat_sector_data)) {
                    return -1;
                }
                
                *(uint16_t*)(fat_sector_data + sector_offset) = new_cluster;
                
                if (ata_write(fat_sector, 1, fat_sector_data)) {
                    return -1;
                }
                
                // Write new cluster's end marker
                fat_sector = fat16_fs.reserved_sectors + 
                            fat * fat16_fs.sectors_per_fat + 
                            (new_cluster * 2) / fat16_fs.bytes_per_sector;
                sector_offset = (new_cluster * 2) % fat16_fs.bytes_per_sector;
                
                if (ata_read(fat_sector, 1, fat_sector_data)) {
                    return -1;
                }
                
                *(uint16_t*)(fat_sector_data + sector_offset) = FAT16_CLUSTER_CHAIN_END;
                
                if (ata_write(fat_sector, 1, fat_sector_data)) {
                    return -1;
                }
            }
            
            current_cluster = new_cluster;
        } else {
            current_cluster = fat_table[current_cluster];
        }
    }

    uint32_t bytes_written = 0;
    const uint8_t* buf_ptr = (const uint8_t*)buffer;
    uint8_t* cluster_buffer = (uint8_t*)malloc(fat16_fs.bytes_per_sector * fat16_fs.sectors_per_cluster);
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
                free(cluster_buffer);
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
            free(cluster_buffer);
            return -1;
        }

        bytes_written += bytes_to_write;
        cluster_offset = 0; // Reset offset for subsequent clusters

        // If we need to write more data, move to or allocate next cluster
        if (bytes_written < size) 
        {
            if (fat_table[current_cluster] >= FAT16_CLUSTER_CHAIN_END) 
            {
                // Allocate new cluster
                uint32_t new_cluster = fat_find_free();
                if (new_cluster == -1) {
                    break; // Can't allocate more space
                }
                
                // Update FAT chain
                fat_table[current_cluster] = new_cluster;
                fat_table[new_cluster] = FAT16_CLUSTER_CHAIN_END;
                
                // Write FAT table updates to disk
                for (int fat = 0; fat < fat16_fs.num_fats; fat++) {
                    // Write current cluster's new value
                    uint32_t fat_sector = fat16_fs.reserved_sectors + 
                                        fat * fat16_fs.sectors_per_fat + 
                                        (current_cluster * 2) / fat16_fs.bytes_per_sector;
                    uint32_t sector_offset = (current_cluster * 2) % fat16_fs.bytes_per_sector;
                    
                    uint8_t fat_sector_data[512];
                    if (ata_read(fat_sector, 1, fat_sector_data)) {
                        free(cluster_buffer);
                        return -1;
                    }
                    
                    *(uint16_t*)(fat_sector_data + sector_offset) = new_cluster;
                    
                    if (ata_write(fat_sector, 1, fat_sector_data)) {
                        free(cluster_buffer);
                        return -1;
                    }
                    
                    // Write new cluster's end marker
                    fat_sector = fat16_fs.reserved_sectors + 
                                fat * fat16_fs.sectors_per_fat + 
                                (new_cluster * 2) / fat16_fs.bytes_per_sector;
                    sector_offset = (new_cluster * 2) % fat16_fs.bytes_per_sector;
                    
                    if (ata_read(fat_sector, 1, fat_sector_data)) {
                        free(cluster_buffer);
                        return -1;
                    }
                    
                    *(uint16_t*)(fat_sector_data + sector_offset) = FAT16_CLUSTER_CHAIN_END;
                    
                    if (ata_write(fat_sector, 1, fat_sector_data)) {
                        free(cluster_buffer);
                        return -1;
                    }
                }
                
                current_cluster = new_cluster;
            } else {
                current_cluster = fat_table[current_cluster];
            }
        }
    }

    // Update file size if needed
    uint32_t new_size = offset + bytes_written;
    if (new_size > file->file_size) 
    {
        file->file_size = new_size;
        
        if (!fat_update_dir_entry(file->name, parent_dir, file)) {
            // Handle error, but don't fail the write operation
            puts("Warning: Failed to update file size in directory entry\n");
        }
    }

    free(cluster_buffer);

    return bytes_written;
}