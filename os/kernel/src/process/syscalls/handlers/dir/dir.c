#include "dir.h"
#include <fcntl.h>
#include "memory/heap/heap.h"
#include "process/manager/process_manager.h"
#include "filesystem/fat/fat.h"
#include "process/syscalls/handlers/file/file.h"

int _chdir(const char *pathname)
{
    process_t* current_process = get_current_process();
    char newPath[256];

    if (pathname[0] == '/')
    {
        strncpy(newPath, pathname, sizeof(newPath));
    }
    else
    {
        join_path(current_process->cwd, pathname, newPath, sizeof(newPath));
    }

    char normalized[256];
    if (normalize_path(newPath, normalized, sizeof(normalized)) == NULL)
    {
        return -ENOMEM;
    }

    FileData tmp_dir;
    if (fat_get_dir_data(normalized, &tmp_dir) != 0)
    {
        return -ENOENT;
    }

    if (tmp_dir.file_entry.attr & FAT_ATTR_DIRECTORY)
    {
        strncpy(current_process->cwd, normalized, sizeof(current_process->cwd));
        return 0;
    }
    else
    {
        return -ENOTDIR;
    }
}

int _mkdir(const char *pathname, mode_t mode)
{
    int code;
    process_t* current_process = get_current_process();
    char newPath[256];

    if (pathname[0] == '/')
    {
        strncpy(newPath, pathname, sizeof(newPath));
    }
    else
    {
        join_path(current_process->cwd, pathname, newPath, sizeof(newPath));
    }

    if((code = fat_create_directory(newPath)) != 0)
    {
        return code;
    }

    return 0;
}

int _rmdir(const char *pathname)
{
    int code;
    process_t* current_process = get_current_process();
    char newPath[256];

    if (pathname[0] == '/')
    {
        strncpy(newPath, pathname, sizeof(newPath));
    }
    else
    {
        join_path(current_process->cwd, pathname, newPath, sizeof(newPath));
    }

    if (get_glob_fd(newPath) != NULL)
    {
        return -EBUSY;
    }

    if((code = fat_delete_dir(newPath)) != 0)
    {
        return code;
    }

    return 0;
}

int _rename(const char *oldpath, const char *newpath)
{
    process_t* current_process = get_current_process();
    char fullOldPath[256];
    char fullNewPath[256];

    if (oldpath[0] == '/')
    {
        strncpy(fullOldPath, oldpath, sizeof(fullOldPath));
    }
    else
    {
        join_path(current_process->cwd, oldpath, fullOldPath, sizeof(fullOldPath));
    }

    join_path(current_process->cwd, newpath, fullNewPath, sizeof(fullNewPath));

    char oldFormattedPath[256];
    char newFormattedPath[256];
    if (normalize_path(fullOldPath, oldFormattedPath, sizeof(oldFormattedPath)) == NULL ||
        normalize_path(fullNewPath, newFormattedPath, sizeof(newFormattedPath)) == NULL)
    {
        return -ENOMEM;
    }

    char newName[12];
    char oldDir[256];
    char newDir[256];
    get_parent_dir(oldFormattedPath, oldDir, sizeof(oldDir));
    get_parent_dir(newFormattedPath, newDir, sizeof(newDir));

    if (strcmp(oldDir, newDir) != 0)
    {
        return -EINVAL;
    }

    get_base_name_with_len(newFormattedPath, newName, 11);
    
    if (fat_rename(oldFormattedPath, newName) != 0)
    {
        return -EINVAL;
    }

    global_file_descriptor* file_fd = get_opened_fd(oldFormattedPath);
    if (file_fd != NULL)
    {
        strncpy(file_fd->path, newFormattedPath, sizeof(file_fd->path));    
    }

    return 0;
}

int _unlink(const char *path)
{
    int r;
    process_t *current_process = get_current_process();
    char full_path[256] = {0};

    if (path[0] == '/')
    {
        strncpy(full_path, path, sizeof(full_path));
    }
    else
    {
        join_path(current_process->cwd, path, full_path, sizeof(full_path));
    }

    FileData tmp = {0};
    if((r = fat_get_file_data(full_path, &tmp)) != 0)
    {
        return r;
    }

    if (get_opened_fd(full_path) == NULL)
    {    
        r = fat_delete_file(full_path);
    }
    else
    {
        r = -EBUSY;
    }

    return r;
}

int _getdents(unsigned int fd, struct linux_dirent *dirp, unsigned int count)
{
    int r;
    int buff_index = 0;
    process_t *current_process = get_current_process();
    FileData dir;
    FAT16_DirEntry entry;
    
    if (fd < 0 || fd >= MAX_LOCAL_FD)
        return -EBADF;
    if (!current_process->fd_table[fd].is_used)
        return -EBADF;
    if (!(current_process->fd_table[fd].flags & O_DIRECTORY))
        return -ENOTDIR;
    if (current_process->fd_table[fd].flags & O_RDONLY)
        return -EPERM;

    if (current_process->fd_table[fd].offset >= current_process->fd_table[fd].global_fd->file.file_entry.file_size)
        return 0;
    
    // get dir entries
    while (count > 0)
    {
        if (current_process->fd_table[fd].offset >= current_process->fd_table[fd].global_fd->file.file_entry.file_size)
            return buff_index;

        if ((r = fat_get_dir_entry(&current_process->fd_table[fd].global_fd->file.file_entry, current_process->fd_table[fd].offset, &entry)) != 0)
            return r;

        int name_len = strlen(entry.name);
        int entry_size = sizeof(struct linux_dirent) + name_len + 1;
        if (entry_size > count)
            break;

        struct linux_dirent* tmp = kmalloc(entry_size);
        if (!tmp) return -ENOMEM;

        tmp->d_ino = entry.start_cluster;
        tmp->d_reclen = entry_size;
        tmp->d_off = current_process->fd_table[fd].offset;
        strcpy(tmp->d_name, entry.name);
        tmp->d_name[name_len] = 0;
        tmp->d_name[name_len + 1] = (entry.attr & FAT_ATTR_DIRECTORY) ? DT_DIR : DT_REG;
        memcpy((void*)((int)dirp + buff_index), tmp, entry_size);
        kfree(tmp);

        current_process->fd_table[fd].offset++;
        buff_index += entry_size;
        count -= entry_size;
    }

    return buff_index;
}

int _getcwd(char *buf, unsigned long size)
{
    process_t *current_process = get_current_process();
    if (strlen(current_process->cwd) + 1 > size)
        return -ERANGE;
    strcpy(buf, current_process->cwd);
    return 0;
}

void join_path(const char* cwd, const char* pathname, char* full_path, size_t size) {
    size_t cwd_len = strlen(cwd);
    size_t path_len = strlen(pathname);
    size_t total_len = cwd_len + path_len + 2; // +2 for '/' and null terminator
    
    if (total_len > size) {
        full_path[0] = '\0';
        return;
    }

    memset(full_path, 0, size);
    
    if (pathname[0] == '/') {
        strncpy(full_path, pathname, path_len + 1);
        return;
    }
    
    strncpy(full_path, cwd, cwd_len);
    full_path[cwd_len] = '/';
    strncpy(full_path + cwd_len + 1, pathname, path_len + 1);
}

char* normalize_path(const char* path, char* normalized, size_t size) {
    if (!path || *path != '/') return NULL;  // Ensure path is absolute

    char** components = kmalloc(sizeof(char*) * MAX_PATH_DEPTH);
    if (!components) {
        return NULL;
    }

    int depth = 0;
    const char* start = path;
    const char* end;

    // Parse path components manually
    while (*start) {
        while (*start == '/') start++;  // Skip leading slashes
        if (*start == '\0') break;      // End of string

        end = start;
        while (*end && *end != '/') end++;  // Find next separator

        int len = end - start;
        if (len == 1 && start[0] == '.') {
            // Skip "."
        } else if (len == 2 && start[0] == '.' && start[1] == '.') {
            // Handle ".." (go back one level)
            if (depth > 0) depth--;
        } else {
            // Copy component into components array
            if (depth < MAX_PATH_DEPTH) {
                components[depth] = kmalloc(len + 1);
                if (!components[depth]) {
                    while (depth--) kfree(components[depth]);
                    kfree(components);
                    return NULL;
                }
                memcpy(components[depth], start, len);
                components[depth][len] = '\0';
                depth++;
            }
        }

        start = end;
    }

    // Rebuild normalized path
    char* ptr = normalized;
    *ptr++ = '/';
    for (int i = 0; i < depth; i++) {
        strcpy(ptr, components[i]);
        ptr += strlen(components[i]);
        if (i < depth - 1) *ptr++ = '/';
        kfree(components[i]);
    }
    *ptr = '\0';

    kfree(components);
    return normalized;
}

void get_parent_dir(const char *path, char *parent_dir, size_t size)
{
    char path_copy[256];
    strncpy(path_copy, path, sizeof(path_copy));

    char *last_slash = strrchr(path_copy, '/');
    if (last_slash) {
        *(last_slash) = '\0';
    }

    char *directory = path_copy;
    
    if (strlen(directory) == 0)
    {
        strncpy(parent_dir, "/", size);
        return;
    }

    strncpy(parent_dir, directory, size);
}

void get_base_name(const char *path, char *name, size_t size)
{
    char path_copy[256];
    strncpy(path_copy, path, sizeof(path_copy));

    char *last_slash = strrchr(path_copy, '/');
    if (last_slash) {
        *last_slash = '\0';
    }

    char *base_name = last_slash ? last_slash + 1 : path_copy;

    strncpy(name, base_name, size);
}

void get_base_name_with_len(const char *path, char *name, int name_len)
{
    char path_copy[256];
    strncpy(path_copy, path, sizeof(path_copy));

    char *last_slash = strrchr(path_copy, '/');
    if (last_slash) {
        *last_slash = '\0';
    }

    char *base_name = last_slash ? last_slash + 1 : path_copy;
    if (strlen(base_name) > name_len)
    {
        strncpy(name, base_name, name_len);
        name[name_len] = '\0'; // Ensure null-termination
        return;
    }
    strncpy(name, base_name, name_len);
}
