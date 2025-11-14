#include "process_manager.h"
#include "memory/heap/heap.h"
#include "process/loader/elf_loader.h"
#include "process/elf/parser.h"
#include "cpu/gdt/gdt.h"
#include "drivers/vga/vga.h"
#include "process/syscalls/handlers/file/file.h"
#include "terminal/terminal_manager.h"

extern void jump_usermode(process_registers_t *addr);
extern void jump_kernelmode(process_registers_t *addr);

#define HIGHER_HALF_START 0xC0000000

static process_node_t* process_list_head = NULL;
static process_node_t* process_list_tail = NULL;
static uint32_t next_pid = 1;
static process_node_t* current_process_g = NULL;
static bool run_processes = false;

static bool manage_initialized = false;

void add_to_linked_list(process_node_t* new_process_node)
{
    if (!new_process_node) return; // Avoid NULL pointer issues

    new_process_node->next = NULL;
    new_process_node->prev = NULL; // Initialize prev to avoid garbage value

    if (process_list_head == NULL)
    {
        process_list_head = new_process_node;
        process_list_tail = new_process_node;
    }
    else
    {
        process_list_tail->next = new_process_node;
        new_process_node->prev = process_list_tail; // Correctly set prev
        process_list_tail = new_process_node;
    }
}

void proc_manager_init()
{
    // TODO: init the fd's
    get_glob_fd()[0].is_used = true;
    get_glob_fd()[1].is_used = true;
    get_glob_fd()[2].is_used = true;

    manage_initialized= true;

    // register force_context_switch interrupt
    register_isr_handler(0x69, switch_process);
}

void init_proc_fd(file_descriptor *fd_table, size_t size)
{
    // TODO: initialzie stdin, stdout, stderr
    // init is from the current active terminal
    fd_table[0].is_used = true;
    fd_table[1].is_used = true;
    fd_table[2].is_used = true;
}

static int remove_from_linked_list(process_node_t* proc_node)
{
    if (!proc_node) return -1; // Handle NULL input
    
    if (proc_node == process_list_head) {
        process_list_head = proc_node->next;
    }

    if (proc_node == process_list_tail) {
        process_list_tail = proc_node->prev;
    }

    if (proc_node->prev) {
        proc_node->prev->next = proc_node->next;
    }
    
    if (proc_node->next) {
        proc_node->next->prev = proc_node->prev;
    }

    return 0;
}

int create_process(const char *path, int flags)
{
    int r = 0;
    if (!manage_initialized)
        return false;

    FileData data = {0};
    if ((r = fat_get_file_data(path, &data)))
    {
        return r;
    }
    kmalloc(1); // TODO: find out why it crashes
    int elf_len = data.file_entry.file_size;
    uint8_t* elf_content = kmalloc(elf_len);
    memset(elf_content, 0, data.file_entry.file_size);

    fat_read(&data.file_entry, 0, data.file_entry.file_size, elf_content);
    
    if (elf_is_valid_and_loadable(elf_content, elf_len))
    {
        if (!create_process_from_elf(elf_content, elf_len, flags))
        {
            kfree(elf_content);
            return -1;
        }
    }
    else
    {
        kfree(elf_content);
        return -1; // TODO: Try load binary
    }

    kfree(elf_content);
    return 0;
}

bool create_process_from_elf(const uint8_t* elf_content, uint32_t elf_len, int flags)
{
    if (!manage_initialized)
        return false;

    if (!elf_is_valid_and_loadable(elf_content, elf_len))
    {
        return false;
    }

    struct page_directory_entry* prev_pd = get_current_pd();
    struct page_directory_entry* kernel_pd = get_kernel_pd();

    load_pd(kernel_pd);

    process_node_t* new_process_node = kmalloc(sizeof(process_node_t));
    if (new_process_node == NULL) 
    {
        return false;
    }

    memset(new_process_node->proc.cwd, 0, 256);
    strcpy(new_process_node->proc.cwd, "/");

    elf_hdr* elf_header = elf_get_header(elf_content);

    new_process_node->proc.pid = next_pid++;
    new_process_node->proc.state = PROCESS_READY;
    new_process_node->proc.regs = (process_registers_t){0};
    
    new_process_node->proc.kernel_stack = kmalloc_pages(PROC_KERNEL_STACK_SIZE) + PAGE_SIZE * PROC_KERNEL_STACK_SIZE;
    new_process_node->proc.page_directory = (struct page_directory_entry*)kmalloc_pages(1);
    if (new_process_node->proc.page_directory == NULL)
    {
        kfree(new_process_node);
        return false;
    }

    // Copy the kernel page directory entries
    // [0x300] - [0x400] entries are for the kernel
    memcpy(&new_process_node->proc.page_directory[HIGHER_HALF_START / PAGE_SIZE / PAGES_PER_DIR],
        &kernel_pd[HIGHER_HALF_START / PAGE_SIZE / PAGES_PER_DIR], 
        sizeof(struct page_directory_entry) * (PAGES_PER_DIR - (HIGHER_HALF_START / PAGE_SIZE / PAGES_PER_DIR)));
    
    // Implement recursive mapping (map the last entry to the page directory itself)
    new_process_node->proc.page_directory[PAGES_PER_DIR - 1].table_entry_address 
        = (get_physical_address(new_process_node->proc.page_directory) >> 12);

    load_pd(new_process_node->proc.page_directory);

    // Load process to memory
    uint32_t process_size = elf_load_process(elf_content, elf_len, new_process_node->proc.page_directory);
    load_pd(kernel_pd);

    if (process_size == 0)
    {
        kfree(new_process_node->proc.page_directory);
        kfree(new_process_node);
        return false;
    }

    init_proc_fd(new_process_node->proc.fd_table, MAX_LOCAL_FD);
    attach_process_to_terminal(get_active_terminal_id(), &new_process_node->proc);
    
    new_process_node->proc.regs.eip = elf_header->e_entry;
    new_process_node->proc.regs.esp = USER_STACK_TOP - 4;
    
    new_process_node->proc.regs.cs = 0x1B;
    new_process_node->proc.regs.ss = 0x23;
    new_process_node->proc.regs.eflags = 0x0202; // interrupt enable flag + reserved flag
    new_process_node->proc.is_kernel_mode = false;
    
    add_to_linked_list(new_process_node);

    load_pd(prev_pd);

    return true;
}

static void free_proc_node(process_t* process)
{
    kfree(process->page_directory); // TODO: Free page tables
    kfree(process);
}

static void jump_proc_wrapper(process_t* proc)
{
    load_pd(proc->page_directory);
    tss_fill_esp0((uint32_t)proc->kernel_stack);

    if (proc->is_kernel_mode)
        jump_kernelmode(&proc->regs);
    else
        jump_usermode(&proc->regs);
}


int exit_proc(process_node_t* exiting_proc)
{
    if (!exiting_proc) return -EINVAL; // Validate input

    // Check if there are any processes left
    if (!process_list_head) {
        panic_screen("No process running, Reboot PC1!!!");
    }

    // Switch to the next process in the list
    if (current_process_g->next) {
        current_process_g = current_process_g->next;
    } else {
        current_process_g = process_list_head;
    }

    if (!current_process_g) {
        panic_screen("No process running, Reboot PC2!!!");
    }

    load_pd(get_kernel_pd());
    remove_from_linked_list(exiting_proc);
    free_proc_node(&exiting_proc->proc);

    jump_proc_wrapper(&current_process_g->proc);
}

void exit_current_process()
{
    process_node_t* exiting_proc = current_process_g;
    exit_proc(exiting_proc);
}

inline process_t* get_current_process()
{
    if (current_process_g)
        return &current_process_g->proc;
    return NULL;
}

inline void force_switch_process()
{
    asm("int $0x69");
}

void wake_up_terminal_processes(uint32_t terminal_id)
{
    process_node_t* iter = current_process_g;

    do {
        if (iter->proc.state == PROCESS_BLOCKED)
        {
            if (iter->proc.terminal_id == terminal_id)
                iter->proc.state = PROCESS_READY;
        }
        if (iter->next != NULL)
        {
            iter = iter->next;
        }
        else
        {
            iter = process_list_head;
        }
    } while(iter != current_process_g);
}

void switch_process(struct int_registers* regs)
{
    if (process_list_head == NULL)
        return;

    if (current_process_g == NULL)
    {
        current_process_g = process_list_head;
        if (current_process_g->proc.state == PROCESS_READY)
            goto switch_label;
    }
    else
    {
        if (current_process_g->proc.state == PROCESS_RUNNING) // only if process was running, set it as ready (if its blocked then dont run ofc)
            current_process_g->proc.state = PROCESS_READY;

        copy_registers(regs, &current_process_g->proc.regs);
        do
        {
            if (current_process_g->next != NULL)
            {
                current_process_g = current_process_g->next;
            }
            else
            {
                current_process_g = process_list_head;
            }
        } while (current_process_g->proc.state != PROCESS_READY);
    }

switch_label:
    current_process_g->proc.state = PROCESS_RUNNING;
    jump_proc_wrapper(&current_process_g->proc);
}


void copy_registers(const struct int_registers *src, process_registers_t *dst) {
    dst->edi = src->edi;
    dst->esi = src->esi;
    dst->ebp = src->ebp;
    dst->unused = 0;  // The `unused` field is not in int_registers, so set it to 0
    dst->ebx = src->ebx;
    dst->edx = src->edx;
    dst->ecx = src->ecx;
    dst->eax = src->eax;
    dst->eip = src->eip;
    dst->cs = src->cs;
    dst->eflags = src->eflags;
    dst->esp = src->esp;
    dst->ss = src->ss;
}

void enable_processes()
{
    if (!manage_initialized)
        return;

    run_processes = true;
}

bool is_schduling()
{
    return run_processes;
}