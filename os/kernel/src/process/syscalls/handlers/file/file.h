#include <stdint.h>
#include <string.h>
#include "process/manager/process_manager.h"

enum lseek_whence_e
{
    SEEK_SET,
    SEEK_CUR,
    SEEK_END
};

typedef enum {
    FILE_TYPE_UNKNOWN = 0,
    FILE_TYPE_REGULAR = 1,   // Regular file (S_IFREG)
    FILE_TYPE_DIRECTORY = 2, // Directory (S_IFDIR)
    FILE_TYPE_CHAR_DEVICE = 3, // Character device (S_IFCHR)
    FILE_TYPE_BLOCK_DEVICE = 4, // Block device (S_IFBLK)
    FILE_TYPE_FIFO = 5,      // FIFO (named pipe) (S_IFIFO)
    FILE_TYPE_SYMLINK = 6,   // Symbolic link (S_IFLNK)
    FILE_TYPE_SOCKET = 7,    // Socket (S_IFSOCK)
} file_type_t;


typedef uint32_t dev_t;
typedef uint32_t ino_t;
typedef uint32_t mode_t;
typedef uint32_t nlink_t;
typedef uint32_t uid_t;
typedef uint32_t gid_t;
typedef uint32_t blksize_t;
typedef uint32_t blkcnt_t;

struct timespec {
    uint32_t tv_sec;
    uint32_t tv_nsec;
};

struct stat {
    dev_t     st_dev;         /* ID of device containing file */
    ino_t     st_ino;         /* Inode number */
    uint32_t  st_mode;        /* File type and mode */
    nlink_t   st_nlink;       /* Number of hard links */
    uid_t     st_uid;         /* User ID of owner */
    gid_t     st_gid;         /* Group ID of owner */
    dev_t     st_rdev;        /* Device ID (if special file) */
    uint32_t  st_size;        /* Total size, in bytes */
    blksize_t st_blksize;     /* Block size for filesystem I/O */
    blkcnt_t  st_blocks;      /* Number of 512B blocks allocated */
    struct timespec    st_atime;       /* Time of last access */
    struct timespec    st_mtime;       /* Time of last modification */
    struct timespec    st_ctime;       /* Time of last status change */
};

global_file_descriptor* get_glob_fd();
global_file_descriptor* get_opened_fd(char *path);

/**
 * _open - Opens a file.
 *
 * @path: The path to the file.
 * @flags: Flags to control how the file is opened.
 *
 * Returns:
 *   A file descriptor on success.
 */
int _open(char *path, uint32_t flags);

/**
 * _close - Closes a file descriptor.
 *
 * @fd: The file descriptor to close.
 *
 * Returns:
 *   0 on success.
 *   -EBADF if the file descriptor is invalid.
 *   -ESRCH if the current process is not found.
 */
int _close(int fd);

/**
 * _read - Reads data from a file descriptor.
 *
 * @fd: The file descriptor to read from.
 * @buf: The buffer to read data into.
 * @count: The number of bytes to read.
 *
 * Returns:
 *   The number of bytes read on success.
 *   -EBADF if the file descriptor is invalid.
 *   -EPERM if the file descriptor is not open for reading.
 *   -ESRCH if the current process is not found.
 */
int _read(int fd, void *buf, uint32_t count);

/**
 * _write - Writes data to a file descriptor.
 *
 * @fd: The file descriptor to write to.
 * @buf: The buffer to write data from.
 * @count: The number of bytes to write.
 *
 * Returns:
 *   The number of bytes written on success.
 *   -EBADF if the file descriptor is invalid.
 *   -EPERM if the file descriptor is not open for writing.
 *   -ESRCH if the current process is not found.
 */
int _write(int fd, void *buf, uint32_t count);

/**
 * _lseek - Repositions the file offset of a file descriptor.
 *
 * @fd: The file descriptor.
 * @offset: The offset to set.
 * @whence: The reference point for the offset (SEEK_SET, SEEK_CUR, SEEK_END).
 *
 * Returns:
 *   The new offset on success.
 *   -EBADF if the file descriptor is invalid.
 *   -EINVAL if the whence argument is invalid or the resulting offset is invalid.
 *   -ESRCH if the current process is not found.
 */
int _lseek(int fd, int offset, int whence);

/**
 * _stat - Retrieves information about a file.
 *
 * @pathname: The path to the file.
 * @statbuf: The buffer to store the file information.
 *
 * Returns:
 *   0 on success.
 *   -ENOMEM if memory allocation fails.
 *   -ENOENT if the file does not exist.
 */
int _stat(const char *pathname, struct stat *statbuf);

/**
 * _fstat - Retrieves information about a file based on its file descriptor.
 *
 * @fd: The file descriptor.
 * @statbuf: The buffer to store the file information.
 *
 * Returns:
 *   0 on success.
 *   -EBADF if the file descriptor is invalid.
 *   -ESRCH if the current process is not found.
 */
int _fstat(int fd, struct stat *statbuf);

int _truncate(const char *path, long length);
int _ftruncate(int fd, long length);