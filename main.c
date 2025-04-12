#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "shell.h"

int main(int argc, char **argv) {
    pid_t childPid;
    char *cmdLine;
    parseInfo *info;
    
    // Main shell loop
    while (1) {
        // Display prompt and read command
        cmdLine = readline("> ");
        
        // Handle EOF or errors
        if (cmdLine == NULL) {
            printf("Exiting shell...\n");
            break;
        }
        
        // Skip empty commands
        if (strlen(cmdLine) == 0) {
            free(cmdLine);
            continue;
        }
        
        // Parse the command
        info = parse(cmdLine);
        
        // Handle built-in commands
        if (info->args[0] != NULL && is_builtin_command(info->args[0])) {
            execute_builtin_command(info);
            free_parse_info(info);
            free(cmdLine);
            continue;
        }
        
        // Fork a child process
        childPid = fork();
        
        if (childPid == 0) {
            // Child process
            executeCommand(info);
            // If executeCommand returns, there was an error
            exit(EXIT_FAILURE);
        } else if (childPid < 0) {
            // Fork failed
            perror("fork");
        } else {
            // Parent process
            int status;
            waitpid(childPid, &status, 0);
        }
        
        // Free memory
        free_parse_info(info);
        free(cmdLine);
    }
    
    return 0;
}