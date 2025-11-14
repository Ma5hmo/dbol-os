#include "proc.h"
#include "process/manager/process_manager.h"

void _exit(int status)
{
    exit_current_process();
}

int _getpid()
{
    return get_current_process()->pid;
}