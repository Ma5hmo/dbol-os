#include "file.h"


int read_fat_fs(void *buf, uint32_t count, uint32_t off, struct global_file_descriptor_t* glob_fd)
{
    return fat_read(&glob_fd->file.file_entry, off, count, buf);
}

global_file_descriptor global_fd_table[MAX_FD] = {0};

global_file_descriptor* get_glob_fd()
{
    return global_fd_table;
}

global_file_descriptor* get_opened_fd(char *path)
{
    for (int i=0; i<MAX_FD; i++)
    {
        if (global_fd_table[i].is_used)
        {    if (strncmp(global_fd_table[i].path, path, sizeof(global_fd_table[i].path)) == 0) 
            {
                return &global_fd_table[i];
            }
        }
    }

    return NULL;
}

global_file_descriptor* allocate_global_fd(char *path)
{
    for (int i=0; i<MAX_FD; i++)
    {
        if (!global_fd_table[i].is_used)
        { 
            memset(&global_fd_table[i], 0 , sizeof(global_file_descriptor));
            strncpy(global_fd_table[i].path, path, sizeof(global_fd_table[i].path) - 1);
            global_fd_table[i]._read = read_fat_fs;
            global_fd_table[i].is_device = false;
            global_fd_table[i].ref_count = 1;
            return &global_fd_table[i];
        }
    }

    return NULL;
}

global_file_descriptor* allocate_device_fd()
{
    for (int i=0; i<MAX_FD; i++)
    {
        if (!global_fd_table[i].is_used)
        { 
            memset(&global_fd_table[i], 0 , sizeof(global_file_descriptor));
            global_fd_table[i].ref_count = 1;
            global_fd_table[i].is_device = true;
            global_fd_table[i].is_used = true;
            return &global_fd_table[i];
        }
    }

    return NULL;
}