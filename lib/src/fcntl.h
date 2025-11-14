#ifndef _FCNTL_H
#define _FCNTL_H

// File access modes (only one of these should be used)
#define O_RDONLY   0x0000  // Read-only
#define O_WRONLY   0x0001  // Write-only
#define O_RDWR     0x0002  // Read and write

// File creation/modification flags
#define O_CREAT    0x0040  // Create file if it doesn't exist
#define O_EXCL     0x0080  // Fail if file already exists (used with O_CREAT)
#define O_TRUNC    0x0200  // Truncate file to zero size if it exists
#define O_APPEND   0x0400  // Append mode (writes go to end of file)

// File behavior flags
#define O_NONBLOCK 0x0800  // Non-blocking mode
#define O_SYNC     0x101000 // Writes are synchronized to disk
#define O_DSYNC    0x1000  // Data-only sync (metadata can be delayed)
#define O_NOFOLLOW 0x2000  // Don't follow symbolic links

// Special device flags
#define O_DIRECTORY 0x10000 // Fail if not a directory
#define O_CLOEXEC   0x80000 // Close on exec()
#define O_TMPFILE   0x400000 // Temporary file (anonymous)

// Enum for easier flag management
typedef enum {
    RDONLY   = O_RDONLY,
    WRONLY   = O_WRONLY,
    RDWR     = O_RDWR,
    CREAT    = O_CREAT,
    EXCL     = O_EXCL,
    TRUNC    = O_TRUNC,
    APPEND   = O_APPEND,
    NONBLOCK = O_NONBLOCK,
    SYNC     = O_SYNC,
    DSYNC    = O_DSYNC,
    NOFOLLOW = O_NOFOLLOW,
    DIRECTORY = O_DIRECTORY,
    CLOEXEC  = O_CLOEXEC,
    TMPFILE  = O_TMPFILE
} OpenFlags;

#endif // _FCNTL_H
