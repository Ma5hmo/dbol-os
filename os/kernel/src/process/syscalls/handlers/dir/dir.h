#pragma once

#include <string.h>
#include <errno-base.h>

typedef uint32_t mode_t;

#define MAX_PATH_DEPTH 256

struct linux_dirent {
    unsigned long  d_ino;     /* Inode number */
    unsigned long  d_off;     /* Not an offset; see below */
    unsigned short d_reclen;  /* Length of this linux_dirent */
    char           d_name[];  /* Filename (null-terminated) */
                              /* length is actually (d_reclen - 2 -
                                offsetof(struct linux_dirent, d_name)) */
    /*
    char           pad;       // Zero padding byte
    char           d_type;    // File type
    */
};

enum D_TYPE
{
  DT_UNKNOWN = 0,
  DT_FIFO = 1,
  DT_CHR = 2,
  DT_DIR = 4,
  DT_BLK = 6,
  DT_REG = 8,
  DT_LNK = 10,
  DT_SOCK = 12,
  DT_WHT = 14
};

/**
 * _chdir - Changes the current working directory of the calling process.
 *
 * @pathname: The path to the new directory.
 *
 * Returns:
 *   0 on success.
 *   -ENOMEM if memory allocation fails.
 *   -ENOENT if the directory does not exist.
 *   -ENOTDIR if the path is not a directory.
 */
int _chdir(const char *pathname);

/**
 * _mkdir - Creates a new directory with the specified pathname and mode.
 *
 * @pathname: The path of the directory to be created.
 * @mode: The mode (permissions) for the new directory.
 *
 * Returns:
 *   0 on success.
 *   -ENOMEM if memory allocation fails.
 *   Other error codes depending on the filesystem implementation.
 */
int _mkdir(const char *pathname, mode_t mode);

/**
 * _rmdir - Removes the directory specified by pathname.
 *
 * @pathname: The path of the directory to be removed.
 *
 * Returns:
 *   0 on success.
 *   -ENOMEM if memory allocation fails.
 *   Other error codes depending on the filesystem implementation.
 */
int _rmdir(const char *pathname);

/**
 * _rename - Renames a file or directory from oldpath to newpath.
 *
 * @oldpath: The current path of the file or directory.
 * @newpath: The new path for the file or directory.
 *
 * Returns:
 *   0 on success.
 *   -ENOMEM if memory allocation fails.
 *   -EINVAL if the new path is invalid or if the old and new paths are in different directories.
 *   Other error codes depending on the filesystem implementation.
 */
int _rename(const char *oldpath, const char *newpath);

int _unlink(const char *path);

int _getdents(unsigned int fd, struct linux_dirent *dirp, unsigned int count);

int _getcwd(char *buf,	unsigned long size);

void join_path(const char* cwd, const char* pathname, char* full_path, size_t size);

char* normalize_path(const char* path, char* normalized, size_t size);

void get_parent_dir(const char *path, char *parent_dir, size_t size);

void get_base_name(const char *path, char *name, size_t size);

void get_base_name_with_len(const char *path, char *name, int name_len);