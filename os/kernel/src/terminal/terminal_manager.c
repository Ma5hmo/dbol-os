#include "terminal_manager.h"
#include "filesystem/vfs/file.h"

#define TERMINAL_AMMOUNT 4
static terminal_struct_t terminals[TERMINAL_AMMOUNT] = {0};

static uint32_t current_active_terminal_id = 0;

bool attach_process_to_terminal(uint32_t terminal_id, process_t *proc_info)
{
    if (terminal_id < 0 || terminal_id > TERMINAL_AMMOUNT)
        false;

    if (terminals[terminal_id - 1].id != terminal_id)
        return false;

    proc_info->terminal_id= terminal_id;

    // initialize process in/out fd's
    proc_info->fd_table[0].global_fd = terminals[terminal_id - 1].terminal_fds.stdin;
    proc_info->fd_table[1].global_fd = terminals[terminal_id - 1].terminal_fds.stdout;
    proc_info->fd_table[2].global_fd = terminals[terminal_id - 1].terminal_fds.stderr;

    return true;
}

uint32_t create_terminal(uint32_t parent_process_id)
{
    int i = 0;
    while (terminals[i].id != 0)
    {
        if (i >= TERMINAL_AMMOUNT)
            return 0;
        i++;
    }

    terminals[i].id = i + 1;
    memset(terminals[i].input_buf, 0, INPUT_BUFFER_SIZE);
    terminals[i].is_input_ready = false;
    terminals[i].parent_process_pid= parent_process_id;
    terminals[i].terminal_fds.stdin = allocate_device_fd();
    terminals[i].terminal_fds.stdout = allocate_device_fd();
    terminals[i].terminal_fds.stderr = allocate_device_fd();
    terminals[i].input_len = 0;

    terminals[i].terminal_fds.stdin->_read = read_terminal_input;

    return i + 1;
}

struct terminal_struct_t* get_active_terminal_struct()
{
    if (current_active_terminal_id == 0)
        return NULL;
    return &terminals[current_active_terminal_id - 1];
}

inline uint32_t get_active_terminal_id()
{
    return current_active_terminal_id;
}

bool set_active_terminal(uint32_t terminal_id)
{
    if (terminal_id < 0 || terminal_id > TERMINAL_AMMOUNT)
        false;

    if (terminals[terminal_id - 1].id != terminal_id)
        return false;

    current_active_terminal_id = terminal_id;

    return true;
}

int read_terminal_input(void *buf, uint32_t count, uint32_t off, struct global_file_descriptor_t* glob_fd)
{
    // yeald blocked until \n pressed
    while (!get_active_terminal_struct()->is_input_ready)
    {
        get_current_process()->state = PROCESS_BLOCKED;
        force_switch_process();
    }
    int copy_len = count;
    get_active_terminal_struct()->is_input_ready = false;
    if (count > get_active_terminal_struct()->input_len)
        copy_len = get_active_terminal_struct()->input_len;

    memcpy(buf ,get_active_terminal_struct()->input_buf, copy_len);
    memset(get_active_terminal_struct()->input_buf, 0, INPUT_BUFFER_SIZE);
    get_active_terminal_struct()->input_len = 0;

    return copy_len;
}