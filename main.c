#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "shell.h"

int main(int argc, char **argv) {
    int childPid;
    char *cmdLine;
    parseInfo *info;
    
    while (1) {
        cmdLine = readline("mysh> ");
        info = parse(cmdLine);

        // Skip empty commands
        if (info->size == 0) {
            free(cmdLine);
            free_parse_info(info);
            continue;
        }

        // Handle built-in commands without forking
        if (isBuiltInCommand(info)) {
            executeBuiltInCommand(info);
            free(cmdLine);
            free_parse_info(info);
            continue;
        }

        // Fork for external commands
        childPid = fork();
        
        if (childPid == 0) {
            /* child code */
            executeCommand(info);
            exit(EXIT_SUCCESS);  // In case execvp fails
        } else {
            /* parent code */
            waitpid(childPid, NULL, 0);
        }
        
        // Free allocated memory
        free(cmdLine);
        free_parse_info(info);
    }
    
    return 0;
}