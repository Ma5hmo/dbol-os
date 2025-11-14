#pragma once
#include "process/manager/process_manager.h"

#define MAX_FD 256

int read_fat_fs(void *buf, uint32_t count, uint32_t off, struct global_file_descriptor_t* glob_fd);

global_file_descriptor* get_opened_fd(char *path);
global_file_descriptor* allocate_global_fd(char *path);
global_file_descriptor* allocate_device_fd();