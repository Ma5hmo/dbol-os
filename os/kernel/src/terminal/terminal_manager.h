#pragma once
#include <stdint.h>
#include "process/manager/process_manager.h"

#define INPUT_BUFFER_SIZE 256

struct terminal_file_descriptors_t {
    struct global_file_descriptor_t* stdin;
    struct global_file_descriptor_t* stdout;
    struct global_file_descriptor_t* stderr;
};

typedef struct terminal_struct_t {
    uint32_t id;
    char input_buf[INPUT_BUFFER_SIZE];
    uint32_t input_len;
    bool is_input_ready;
    uint32_t parent_process_pid;
    struct terminal_file_descriptors_t terminal_fds;
} terminal_struct_t;

bool attach_process_to_terminal(uint32_t terminal_id, process_t *proc_info);

uint32_t create_terminal(uint32_t parent_process_id);
struct terminal_struct_t* get_active_terminal_struct();
uint32_t get_active_terminal_id();
bool set_active_terminal(uint32_t terminal_id);
int read_terminal_input(void *buf, uint32_t count, uint32_t off, struct global_file_descriptor_t* glob_fd);