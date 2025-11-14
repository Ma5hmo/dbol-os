#include "syscalls.h"
#include "sys/sys.h"
#include "drivers/vga/vga.h"
#include "cpu/gdt/gdt.h"
#include "process/manager/process_manager.h"

static void (*syscall_handler_array[SYSCALLS_MANAGER_MAX_HANDLERS])(struct int_registers *registers);

static void handle_syscall(struct int_registers* registers);


void syscall_init()
{
    // attach syscall manager
    register_isr_handler(0x80, handle_syscall);

    syscalls_manager_attach_handler(1, sys_exit);
    syscalls_manager_attach_handler(3, sys_read);
    syscalls_manager_attach_handler(4, sys_write);
    syscalls_manager_attach_handler(5, sys_open);
    syscalls_manager_attach_handler(6, sys_close);
    syscalls_manager_attach_handler(10, sys_unlink);
    syscalls_manager_attach_handler(12, sys_chdir);
    syscalls_manager_attach_handler(19, sys_lseek);
    syscalls_manager_attach_handler(38, sys_rename);
    syscalls_manager_attach_handler(39, sys_mkdir);
    syscalls_manager_attach_handler(40, sys_rmdir);
    syscalls_manager_attach_handler(40, sys_times);
    syscalls_manager_attach_handler(78, sys_gettimeofday);
    syscalls_manager_attach_handler(92, sys_truncate);
    syscalls_manager_attach_handler(93, sys_ftruncate);
    syscalls_manager_attach_handler(106, sys_stat);
    syscalls_manager_attach_handler(108, sys_fstat);
    syscalls_manager_attach_handler(141, sys_getdents);
    syscalls_manager_attach_handler(183, sys_getcwd);

}


static void handle_syscall(struct int_registers* registers)
{
    if (registers->eax < SYSCALLS_MANAGER_MAX_HANDLERS && syscall_handler_array[registers->eax] != 0)
    {
        get_current_process()->is_kernel_mode = true;
        (*syscall_handler_array[registers->eax])(registers);
        get_current_process()->is_kernel_mode = false;
        tss_fill_esp0((uint32_t)get_current_process()->kernel_stack);
    }
}

void syscalls_manager_attach_handler(uint8_t function_number, void (*handler)(int_registers *state))
{
    if (function_number < SYSCALLS_MANAGER_MAX_HANDLERS) 
    {
        syscall_handler_array[function_number] = handler;
    }
}

void syscalls_manager_detach_handler(uint8_t function_number)
{
    if (function_number < SYSCALLS_MANAGER_MAX_HANDLERS)
    {
        syscall_handler_array[function_number] = 0;
    }
}
