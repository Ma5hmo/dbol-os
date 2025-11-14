#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>

#define MAX_INPUT_LENGTH 256
#define MAX_ARGS 64
#define MAX_PATH 256
#define MAX_BUFFER_SIZE 4096

typedef int (*cmd_func)(char **args);

int execute_command(char **args);
int parse_input(char *input, char *arg_buffer, char **args);
int start_shell();
void print_cwd();

int cmd_help(char **args);
int cmd_ls(char **args);
int cmd_cd(char **args);
int cmd_cat(char **args);
int cmd_echo(char **args);
int cmd_mkdir(char **args);
int cmd_rm(char **args);
int cmd_rmdir(char **args);
int cmd_exit(char **args);

char *supported_commands[] = {
    "help",
    "ls",
    "cd",
    "cat",
    "echo",
    "mkdir",
    "rm",
    "rmdir",
    "exit"
};

cmd_func command_funcs[] = {
    &cmd_help,
    &cmd_ls,
    &cmd_cd,
    &cmd_cat,
    &cmd_echo,
    &cmd_mkdir,
    &cmd_rm,
    &cmd_rmdir,
    &cmd_exit
};

int num_cmds()
{
    return sizeof(supported_commands)/sizeof(char *);
}

int main() 
{
    int r = start_shell();
    return r;
}

int start_shell()
{
    char input[MAX_INPUT_LENGTH] = {0};
    // Buffer to store copies of arguments
    char arg_buffer[MAX_INPUT_LENGTH] = {0};
    char *args[MAX_ARGS] = {0};
    int status = 1;

    while (status)
    {
        print_cwd();
        if(fgets(input, MAX_INPUT_LENGTH, stdin) == NULL)
        {
            printf("fgets error");
            continue;
        }

        // Remove trailing newline
        size_t len = strlen(input);
        if (len > 0 && input[len-1] == '\n') {
            input[len-1] = '\0';
        }

        if (parse_input(input, arg_buffer, args) == 0) {
            continue;  // Empty input
        }

        status = execute_command(args);
    }
    
    return 0;
}

void print_cwd()
{
    char cwd[MAX_PATH] = {0};

    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        printf("Unknown path");
        exit(1);
    }
    printf("DBOLOS:%s > ", cwd);
}

int execute_command(char **args)
{
    if (args[0] == NULL)
    {
        return 1;
    }

    // check for the command
    for (int i=0; i<num_cmds(); ++i)
    {
        if (strcmp(supported_commands[i], args[0]) == 0)
            return command_funcs[i](args);
    }

    printf("Unknown command: %s\n", args[0]);
    return 1;
}

// Custom parsing function that doesn't use strtok
int parse_input(char *input, char *arg_buffer, char **args) {
    int arg_count = 0;
    int buf_pos = 0;
    int i = 0;
    int input_len = strlen(input);
    
    // Skip leading whitespace
    while (i < input_len && isspace((unsigned char)input[i])) {
        i++;
    }
    
    while (i < input_len && arg_count < MAX_ARGS - 1) {
        // Mark start of argument
        args[arg_count] = &arg_buffer[buf_pos];
        
        // Copy characters until whitespace
        while (i < input_len && !isspace((unsigned char)input[i])) {
            arg_buffer[buf_pos++] = input[i++];
        }
        
        // Null-terminate this argument
        arg_buffer[buf_pos++] = '\0';
        arg_count++;
        
        // Skip whitespace to next argument
        while (i < input_len && isspace((unsigned char)input[i])) {
            i++;
        }
    }
    
    args[arg_count] = NULL;  // Null-terminate the args list
    return arg_count;
}

int cmd_help(char **args)
{
    printf("Welcome to DBOLOS hacker terminal!!\n");
    printf("We support a lot of commands:\n");

    for (int i=0; i<num_cmds(); ++i)
    {
        printf("-   %s\n", supported_commands[i]);
    }

    printf("Enjoy!\n");

    return 1;
}

int cmd_ls(char **args)
{
    DIR* dir;
    struct dirent *entry;
    
    // Default to current directory if no argument provided
    const char *path = args[1] ? args[1] : ".";
    
    if ((dir = opendir(path)) == NULL) {
        perror("ls");
        return 1;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        printf("%s\n", entry->d_name);
    }
    
    closedir(dir);

    return 1;
}

int cmd_cd(char **args)
{   
    // if no args then move to root dir '/'
    if (args[1] == NULL)
    {
        if(chdir("/"))
        {
            perror("chdir");
        }
    }
    else
    {
        if(chdir(args[1]))
        {
            perror("chdir");
        }
    }

    return 1;
}

int cmd_cat(char **args)
{
    if (args[1] == NULL)
    {
        printf("cat: missing file\n");
        return 1;
    }

    FILE* fp = fopen(args[1], "rb");

    if (!fp) {
        perror("cat");
        return 1;
    }       

    // Use a fixed-size buffer on the stack
    char buffer[MAX_BUFFER_SIZE];
    size_t bytes_read;
    
    // Read and print the file in chunks
    while ((bytes_read = fread(buffer, 1, MAX_BUFFER_SIZE, fp)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            if (buffer[i] == '\0') {
                putchar(0); // Print \0 explicitly
            } else {
                putchar(buffer[i]);
            }
        }
    }
    putchar('\n');
    
    fclose(fp);
    return 1;
}

int cmd_echo(char **args)
{
    FILE* fd = stdout;
    char buffer[MAX_INPUT_LENGTH] = {0};
    int i = 1;
    
    if (args[1] == NULL)
    {
        printf("\n");
        return 1;
    }
    
    // First pass: find redirection if any
    while(args[i] != NULL)
    {
        if (strcmp(args[i], ">") == 0)
        {
            if (args[i+1] == NULL)
            {
                printf("echo: syntax error, expected file name\n");
                return 1;
            }

            fd = fopen(args[i+1], "w+");
            if (fd == NULL) {
                perror("echo");
                return 1;
            }
            args[i] = NULL; // Mark end of content
            break;
        }
        else if (strcmp(args[i], ">>") == 0)
        {
            if (args[i+1] == NULL)
            {
                printf("echo: syntax error, expected file name\n");
                return 1;
            }

            fd = fopen(args[i+1], "a");
            if (fd == NULL) {
                perror("echo");
                return 1;
            }
            args[i] = NULL; // Mark end of content
            break;
        }
        i++;
    }
    
    // Second pass: build the output string
    buffer[0] = '\0';
    i = 1;
    while(args[i] != NULL)
    {
        if (i > 1) {
            strcat(buffer, " ");
        }
        strcat(buffer, args[i]);
        i++;
    }

    fwrite(buffer, sizeof(char), strlen(buffer), fd);
    if (fd != stdout)
        fclose(fd);
    else
        printf("\n");

    return 1;
}

int cmd_mkdir(char **args)
{
    if (args[1] == NULL)
    {
        printf("mkdir: mkdir <name>\n");
        return 1;
    }

    if (mkdir(args[1], 0755))
        perror("mkdir");

    return 1;
}

int cmd_rm(char **args)
{
    if (args[1] == NULL)
    {
        printf("rm: rm <name>\n");
        return 1;
    }

    if (unlink(args[1]))
        perror("rm");

    return 1;
}

int cmd_rmdir(char **args)
{
    if (args[1] == NULL)
    {
        printf("rmdir: rmdir <name>\n");
        return 1;
    }

    if (rmdir(args[1]))
        perror("rmdir");

    return 1;
}

int cmd_exit(char **args)
{
    return 0;
}