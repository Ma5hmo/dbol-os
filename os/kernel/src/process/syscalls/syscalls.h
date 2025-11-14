#pragma once
#include "cpu/idt/isr.h"

#define SYSCALLS_MANAGER_MAX_HANDLERS 256

void syscall_init();
void syscalls_manager_attach_handler(uint8_t function_number, void (*handler)(struct int_registers *state));
void syscalls_manager_detach_handler(uint8_t function_number);