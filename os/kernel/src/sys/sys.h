#pragma once

#include "cpu/idt/isr.h"

void sys_exit(struct int_registers *state);          // 1
void sys_read(struct int_registers *state);          // 3
void sys_write(struct int_registers *state);         // 4
void sys_open(struct int_registers *state);          // 5
void sys_close(struct int_registers *state);         // 6
void sys_unlink(struct int_registers *state);        // 10
void sys_chdir(struct int_registers *state);         // 12
void sys_lseek(struct int_registers *state);         // 19
void sys_getpid(struct int_registers *state);        // 20
void sys_rename(struct int_registers *state);        // 38
void sys_mkdir(struct int_registers *state);         // 39
void sys_rmdir(struct int_registers *state);         // 40
void sys_times(struct int_registers *state);         // 43
void sys_gettimeofday(struct int_registers *state);  // 78
void sys_truncate(struct int_registers *state);      // 92
void sys_ftruncate(struct int_registers *state);     // 93
void sys_stat(struct int_registers *state);          // 106
void sys_fstat(struct int_registers *state);         // 108
void sys_getdents(struct int_registers *state);      // 141
void sys_getcwd(struct int_registers *state);        // 183