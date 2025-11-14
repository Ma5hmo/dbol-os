#include "sys.h"
#include "process/syscalls/handlers/file/file.h"
#include "process/syscalls/handlers/dir/dir.h"
#include "process/syscalls/handlers/proc/proc.h"
#include "process/syscalls/handlers/time/time.h"

void sys_exit(struct int_registers *state)
{
    _exit(state->ebx);
}

void sys_read(struct int_registers *state)
{
    // First argument (fd) in ebx, second (buffer) in ecx, third (count) in edx
    state->eax = _read(state->ebx, (void*)state->ecx, state->edx);
}

void sys_write(struct int_registers *state)
{
    // First argument (fd) in ebx, second (buffer) in ecx, third (count) in edx
    state->eax = _write(state->ebx, (char*)state->ecx, state->edx);
}

void sys_open(struct int_registers *state)
{
    // First argument (filename) in ebx, second (flags) in ecx
    state->eax = _open((char*)state->ebx, state->ecx);
}

void sys_close(struct int_registers *state)
{
    // First argument (fd) in ebx
    state->eax = _close(state->ebx);
}

void sys_unlink(struct int_registers *state)
{
    // First argument (path) in ebx
    state->eax = _unlink((const char*)state->ebx);
}

void sys_chdir(struct int_registers *state)
{
    // First argument (path) in ebx
    state->eax = _chdir((const char*)state->ebx);
}

void sys_lseek(struct int_registers *state)
{
    // First argument (fd) in ebx, second (offset) in ecx, third (whence) in edx
    state->eax = _lseek(state->ebx, state->ecx, state->edx);
}

void sys_getpid(struct int_registers *state)
{
    state->eax = _getpid();
}

void sys_rename(struct int_registers *state)
{
    // First argument (old path) in ebx, second (new path) in ecx
    state->eax = _rename((const char*)state->ebx, (const char*)state->ecx);
}

void sys_mkdir(struct int_registers *state)
{
    // First argument (path) in ebx, second (mode) in ecx
    state->eax = _mkdir((const char*)state->ebx, state->ecx);
}

void sys_rmdir(struct int_registers *state)
{
    // First argument (path) in ebx
    state->eax = _rmdir((const char*)state->ebx);
}

void sys_times(struct int_registers *state)
{
    state->eax = _times((struct tms *)state->ebx);
}

void sys_gettimeofday(struct int_registers *state)
{
    state->eax = _gettimeofday((struct timeval *)state->ebx, (struct timezone *)state->ecx);
}

void sys_truncate(struct int_registers *state)
{
    // First argument (filename) in ebx, second (length) in ecx
    state->eax = _truncate((const char*)state->ebx, state->ecx);
}

void sys_ftruncate(struct int_registers *state)
{
    // First argument (fd) in ebx, second (length) in ecx
    state->eax = _ftruncate(state->ebx, state->ecx);
}

void sys_stat(struct int_registers *state)
{
    // First argument (filename) in ebx, second (stat struct) in ecx
    state->eax = _stat((void*)state->ebx, (void*)state->ecx);
}

void sys_fstat(struct int_registers *state)
{
    // First argument (fd) in ebx, second (stat struct) in ecx
    state->eax = _fstat(state->ebx, (struct stat*)state->ecx);
}

void sys_getdents(struct int_registers *state)
{
    // First argument (fd) in ebx, second (dirent buffer) in ecx, third (size) in edx
    state->eax = _getdents(state->ebx, (struct linux_dirent*)state->ecx, state->edx);
}

void sys_getcwd(struct int_registers *state)
{
    // First argument (buffer) in ebx, second (buffer size) in ecx
    state->eax = _getcwd((char*)state->ebx, state->ecx);
}