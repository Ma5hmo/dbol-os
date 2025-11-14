#include "file.h"
#include "process/syscalls/handlers/dir/dir.h"
#include "memory/heap/heap.h"
#include "errno-base.h"
#include "drivers/vga/vga.h"
#include "filesystem/vfs/file.h"
#include <fcntl.h>

int _open(char *path, uint32_t flags)
{
    process_t* current_process = get_current_process();
    if (current_process == NULL)
        return -ESRCH;
    char full_path[256] = {0};

    if (path[0] == '/')
    {
        strncpy(full_path, path, sizeof(full_path) - 1);
    }
    else
    {
        join_path(current_process->cwd, path, full_path, sizeof(full_path));
        if (full_path[0] == '\0')
            return -ENOMEM;
    }

    if (full_path == NULL)
        return -ENOMEM;

    for (int i = 0; i < MAX_FD; i++)
    {
        if (!current_process->fd_table[i].is_used)
        {   
            memset(&current_process->fd_table[i], 0, sizeof(current_process->fd_table[i]));
            current_process->fd_table[i].global_fd = get_opened_fd(full_path);

            if (current_process->fd_table[i].global_fd == NULL)
            {
                current_process->fd_table[i].global_fd = allocate_global_fd(full_path);
                if (!(flags & O_DIRECTORY))
                {    
                    if(fat_get_file_data(full_path, &current_process->fd_table[i].global_fd->file) != 0)
                    {
                        if (flags & O_CREAT)
                        {
                            if (fat_create_file(full_path) != 0) 
                            {
                                return -ENOENT; // Failed to create file
                            }
                            
                            // Retrieve newly created file data
                            if (fat_get_file_data(full_path, &current_process->fd_table[i].global_fd->file) != 0) 
                            {
                                return -ENOENT;
                            }     
                        }
                        else
                        {
                            return -ENOENT; // File not found, and O_CREAT was not set
                        }
                    }
                

                    current_process->fd_table[i].global_fd->is_dir = false;
                }
                else
                {
                    if (fat_get_dir_data(full_path, &current_process->fd_table[i].global_fd->file) != 0) 
                    {
                        return -ENOENT;
                    } 
                    current_process->fd_table[i].global_fd->is_dir = true;
                }
            }
            else
            {
                if (current_process->fd_table[i].global_fd->is_dir && !(flags & O_DIRECTORY))
                    return -EISDIR;
                if (!current_process->fd_table[i].global_fd->is_dir && flags & O_DIRECTORY)
                    return -ENOTDIR;

                current_process->fd_table[i].global_fd->ref_count++;
            }

            if (!(flags & O_DIRECTORY))
            {    if (flags & O_EXCL)
                {
                    return -EEXIST; // File already exists
                }

                if (flags & O_TRUNC)
                {
                    fat_truncate(&current_process->fd_table[i].global_fd->file, 0);
                }

                if (flags & O_APPEND)
                {
                    current_process->fd_table[i].offset = current_process->fd_table[i].global_fd->file.file_entry.file_size;
                }
                else
                {
                    current_process->fd_table[i].offset = 0;
                }
            }

            current_process->fd_table[i].global_fd->is_used = true;
            current_process->fd_table[i].is_used = true;
            current_process->fd_table[i].flags = flags;
            return i;
        }
    }
    return -EMFILE;
}


int _close(int fd)
{
    process_t* current_process = get_current_process();
    if (current_process == NULL)
        return -ESRCH;

    if (fd < 0 || fd >= MAX_FD)
        return -EBADF;

    // dont allow closing stdin or stdout
    if (fd == 0 || fd == 1)
        return -EBADF;
    
    if (!current_process->fd_table[fd].is_used)
        return -EBADF;
    
    // decremet ref count from global fd
    current_process->fd_table[fd].global_fd->ref_count--;

    // check if ref count is 0
    if(current_process->fd_table[fd].global_fd->ref_count == 0)
    {
        memset(current_process->fd_table[fd].global_fd, 0, sizeof(global_file_descriptor));
    }

    memset(&current_process->fd_table[fd], 0, sizeof(file_descriptor));
    return 0;
}

int _read(int fd, void* buf, uint32_t count)
{
    process_t* current_process = get_current_process();
    if (current_process == NULL)
        return -ESRCH;
    
    if (fd < 0 || fd >= MAX_FD)
        return -EBADF;

    if (!current_process->fd_table[fd].is_used)
        return -EBADF;

    if (current_process->fd_table[fd].flags & O_DIRECTORY)
        return -EISDIR;
    
    if (current_process->fd_table[fd].flags & O_RDONLY)
        return -EPERM;

    if (current_process->fd_table[fd].global_fd == NULL)
        return -EBADF;
    
    uint32_t bytes_read = current_process->fd_table[fd].global_fd->_read(buf, count, current_process->fd_table[fd].offset, current_process->fd_table[fd].global_fd);
    current_process->fd_table[fd].offset += bytes_read;
    return bytes_read;
}

int _write(int fd, void* buf, uint32_t count)
{
    process_t* current_process = get_current_process();
    if (current_process == NULL)
        return -ESRCH;
    file_descriptor* curr_fd_table = current_process->fd_table;

    if (fd == 0 || fd == 2)
        return -EBADF;

    // stdout
    if (fd == 1)
    {
        char* str = (char*)buf;
        for (uint32_t i = 0; i < count; i++) {
            vga_putchar(str[i]);
        }
        return count;  // Return number of bytes written
    }
    
    if (fd < 0 || fd >= MAX_FD)
        return -EBADF;

    if (!curr_fd_table[fd].is_used)
        return -EBADF;

    if (curr_fd_table[fd].flags & O_DIRECTORY)
        return -EISDIR;
    
    if (curr_fd_table[fd].flags & O_RDONLY)
        return -EPERM;
    
    int bytes_written = fat_write(&curr_fd_table[fd].global_fd->file.file_entry, &curr_fd_table[fd].global_fd->file.parent_entry, curr_fd_table[fd].offset, count, buf);
    
    if (bytes_written < 0)
        return EAGAIN;
    
    curr_fd_table[fd].offset += bytes_written;
    return bytes_written;
}

int _lseek(int fd, int offset, int whence)
{
    int new_offset;
    process_t* current_process = get_current_process();
    if (current_process == NULL)
        return -ESRCH;
    file_descriptor* curr_fd_table = current_process->fd_table;

    if (fd == 0 || fd == 1 || fd == 2)
        return -EBADF;
    
    if (fd < 0 || fd >= MAX_FD)
        return -EBADF;

    if (!curr_fd_table[fd].is_used)
        return -EBADF;
    
    switch (whence)
    {
        case SEEK_SET:
            new_offset = 0;
            break;
        case SEEK_CUR:
            new_offset = curr_fd_table[fd].offset + offset;
            break;
        case SEEK_END:
            new_offset = curr_fd_table[fd].global_fd->file.file_entry.file_size; // dont support seeking past the end of the file
            break;
        default:
            return -EINVAL;
    }

    // dont support seeking past the end of the file
    if (new_offset < 0)
        return -EINVAL;

    if (curr_fd_table[fd].flags & O_DIRECTORY)
    {
        if (new_offset > curr_fd_table[fd].global_fd->file.file_entry.file_size)
            return -EINVAL;
    }

    curr_fd_table[fd].offset = new_offset;

    return curr_fd_table[fd].offset;
}

int _stat(const char *pathname, struct stat *statbuf)
{
    FileData tmp_file;
    process_t* current_process = get_current_process();
    char full_path[256] = {0};

    if (pathname[0] == '/')
    {
        strncpy(full_path, pathname, sizeof(full_path));
    }
    else
    {
        join_path(current_process->cwd, pathname, full_path, sizeof(full_path));
        if (full_path[0] == '\0')
            return -ENOMEM;
    }

    if (full_path == NULL)
        return -ENOMEM;
    
    if (fat_get_file_data(full_path, &tmp_file) != 0 && fat_get_dir_data(full_path, &tmp_file) != 0)
    {    
        return -ENOENT;
    }
    
    statbuf->st_size = tmp_file.file_entry.file_size;
    statbuf->st_mode = tmp_file.file_entry.attr;
    statbuf->st_blocks = (tmp_file.file_entry.file_size + 511) / 512;
    statbuf->st_blksize = 512; // Standard block size
    statbuf->st_nlink = 1;     // FAT doesn't support hard links
    statbuf->st_uid = 0;       // Owner ID
    statbuf->st_gid = 0; 
    return 0;
}

int _fstat(int fd, struct stat *statbuf)
{
    process_t* current_process = get_current_process();
    if (current_process == NULL)
        return -ESRCH;
    file_descriptor* curr_fd_table = current_process->fd_table;

    if (fd == 0 || fd == 1 || fd == 2)
        return -EBADF;
    
    if (fd < 0 || fd >= MAX_FD)
        return -EBADF;

    if (!curr_fd_table[fd].is_used)
        return -EBADF;
    
    statbuf->st_size = curr_fd_table[fd].global_fd->file.file_entry.file_size;
    statbuf->st_mode = curr_fd_table[fd].global_fd->is_device? FILE_TYPE_CHAR_DEVICE :  
                        (curr_fd_table[fd].global_fd->file.file_entry.attr & FAT_ATTR_DIRECTORY)? FILE_TYPE_DIRECTORY : FILE_TYPE_REGULAR;
    statbuf->st_blocks = (curr_fd_table[fd].global_fd->file.file_entry.file_size + 511) / 512;
    statbuf->st_blksize = 512; // Standard block size
    statbuf->st_nlink = 1;     // FAT doesn't support hard links
    statbuf->st_uid = 0;       // Owner ID
    statbuf->st_gid = 0; 
    return 0;
}


int _truncate(const char *path, long length)
{
    int r;
    process_t* current_process = get_current_process();
    char full_path[256] = {0};

    if (path[0] == '/')
    {
        strncpy(full_path, path, sizeof(full_path));
    }
    else
    {
        join_path(current_process->cwd, path, full_path, sizeof(full_path));
    }

    if (full_path == NULL)
        return -ENOMEM;

    FileData tmp_file;
    if (fat_get_file_data(full_path, &tmp_file) != 0)
    {
        return -ENOENT;
    }
    r = fat_truncate(&tmp_file, length);
    if (r == 0)
    {
        tmp_file.file_entry.file_size = length;
    }
    return r;
}


int _ftruncate(int fd, long length)
{
    int r;
    process_t* current_process = get_current_process();
    if (current_process == NULL)
        return -ESRCH;
    file_descriptor* curr_fd_table = current_process->fd_table;

    if (fd == 0 || fd == 1 || fd == 2)
        return -EBADF;
    
    if (fd < 0 || fd >= MAX_FD)
        return -EBADF;

    if (!curr_fd_table[fd].is_used)
        return -EBADF;

    r = fat_truncate(&curr_fd_table[fd].global_fd->file, length);
    if (r == 0)
    {
        curr_fd_table[fd].global_fd->file.file_entry.file_size = length;
    }

    return r;
}