#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include "shell.h"

#define MAX_PATH_LENGTH 1024

/* Print tree structure of directory */
void print_tree(const char *path, int level) {
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char subpath[MAX_PATH_LENGTH];

    // Open directory
    if ((dir = opendir(path)) == NULL) {
        perror("opendir");
        return;
    }

    // Read directory entries
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and .. directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Create full path
        snprintf(subpath, MAX_PATH_LENGTH, "%s/%s", path, entry->d_name);

        // Get file status
        if (lstat(subpath, &statbuf) == -1) {
            perror("lstat");
            continue;
        }

        // Print indentation
        for (int i = 0; i < level; i++) {
            printf("│   ");
        }

        // Print entry name
        if (S_ISDIR(statbuf.st_mode)) {
            printf("├── %s/\n", entry->d_name);
            // Recursively print subdirectory
            print_tree(subpath, level + 1);
        } else {
            printf("├── %s\n", entry->d_name);
        }
    }

    closedir(dir);
}

/* Execute tree command */
void execute_tree(parseInfo *info) {
    const char *path = ".";
    if (info->size > 1) {
        path = info->tokens[1];
    }
    printf("%s\n", path);
    print_tree(path, 0);
}

/* Read a line from stdin with a prompt */
char* readline(char *prompt) {
    char *line = NULL;
    size_t bufsize = 0;
    
    printf("%s", prompt);
    fflush(stdout);
    
    if (getline(&line, &bufsize, stdin) == -1) {
        if (feof(stdin)) {
            printf("\n");
            exit(0);
        } else {
            perror("readline");
            exit(1);
        }
    }
    
    // Remove trailing newline
    line[strlen(line) - 1] = '\0';
    return line;
}

/* Parse the command line into tokens */
parseInfo* parse(char *cmdLine) {
    parseInfo *info = (parseInfo *)malloc(sizeof(parseInfo));
    if (info == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    
    // Initialize tokens array
    info->tokens = (char **)malloc(MAX_ARGS * sizeof(char *));
    if (!info->tokens) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    
    // Initialize pipe and redirect fields
    info->has_pipe = 0;
    info->pipe_index = -1;
    info->has_redirect = 0;
    info->redirect_index = -1;
    info->redirect_file = NULL;
    
    // Make a copy of cmdLine since strtok modifies the string
    char *cmdLine_copy = strdup(cmdLine);
    if (!cmdLine_copy) {
        perror("strdup");
        exit(EXIT_FAILURE);
    }
    
    //count tokens and detect pipe/redirect
    char *token = strtok(cmdLine_copy, DELIMITERS);
    int position = 0;
    
    while (token != NULL && position < MAX_ARGS) {
        if (strcmp(token, "|") == 0) {
            info->has_pipe = 1;
            info->pipe_index = position;
        } else if (strcmp(token, ">") == 0) {
            info->has_redirect = 1;
            info->redirect_index = position;
        }
        position++;
        token = strtok(NULL, DELIMITERS);
    }
    
    free(cmdLine_copy);
    
    //store the tokens
    position = 0;
    token = strtok(cmdLine, DELIMITERS);
    
    while (token != NULL && position < MAX_ARGS) {
        if (strcmp(token, ">") == 0) {
            // Get the next token as redirect file
            token = strtok(NULL, DELIMITERS);
            if (token != NULL) {
                info->redirect_file = token;
                info->tokens[position] = NULL;
                position++;
                break;
            }
        } else if (strcmp(token, "|") == 0) {
            info->tokens[position] = NULL;
            position++;
        } else {
            info->tokens[position] = token;
            position++;
        }
        token = strtok(NULL, DELIMITERS);
    }
    
    info->size = position;
    info->tokens[position] = NULL;
    
    return info;
}

void free_parse_info(parseInfo *info) {
    if (info) {
        free(info->tokens);
        free(info);
    }
}

/* Check if command is built-in */
int isBuiltInCommand(parseInfo *info) {
    if (info == NULL || info->tokens[0] == NULL)
        return 0;
    
    return (strcmp(info->tokens[0], "cd") == 0 ||
            strcmp(info->tokens[0], "exit") == 0 ||
            strcmp(info->tokens[0], "tree") == 0);
}

/* Execute built-in command */
int executeBuiltInCommand(parseInfo *info) {
    if (strcmp(info->tokens[0], "cd") == 0) {
        if (info->size < 2) {
            // No argument provided, change to home directory
            const char *home = getenv("HOME");
            if (home == NULL) {
                fprintf(stderr, "cd: HOME not set\n");
                return -1;
            }
            if (chdir(home) != 0) {
                perror("cd");
                return -1;
            }
        } else {
            if (chdir(info->tokens[1]) != 0) {
                perror("cd");
                return -1;
            }
        }
        return 0;
    }
    
    if (strcmp(info->tokens[0], "exit") == 0) {
        exit(0);
    }

    if (strcmp(info->tokens[0], "tree") == 0) {
        execute_tree(info);
        return 0;
    }
    
    return -1;  // Not a built-in command
}

/* Execute the parsed command */
void executeCommand(parseInfo *info) {
    if (info == NULL || info->size == 0) {
        return;  // Nothing to execute
    }

    // Handle pipe
    if (info->has_pipe) {
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }

        // Create arrays for the two commands
        char **cmd1 = (char **)malloc((info->pipe_index + 1) * sizeof(char *));
        char **cmd2 = (char **)malloc((info->size - info->pipe_index) * sizeof(char *));
        
        // Copy first command
        for (int i = 0; i < info->pipe_index; i++) {
            cmd1[i] = info->tokens[i];
        }
        cmd1[info->pipe_index] = NULL;
        
        // Copy second command
        int cmd2_index = 0;
        for (int i = info->pipe_index + 1; i < info->size && info->tokens[i] != NULL; i++) {
            cmd2[cmd2_index++] = info->tokens[i];
        }
        cmd2[cmd2_index] = NULL;

        // Create child process for first command
        pid_t pid1 = fork();
        if (pid1 < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (pid1 == 0) {  // First child process
            close(pipefd[0]);  // Close read end
            dup2(pipefd[1], STDOUT_FILENO);  // Redirect stdout to pipe
            close(pipefd[1]);

            // Execute first command
            execvp(cmd1[0], cmd1);
            perror("execvp");
            exit(EXIT_FAILURE);
        }

        // Create child process for second command
        pid_t pid2 = fork();
        if (pid2 < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (pid2 == 0) {  // Second child process
            close(pipefd[1]);  // Close write end
            dup2(pipefd[0], STDIN_FILENO);  // Redirect stdin to pipe
            close(pipefd[0]);

            // Handle redirection if present
            if (info->has_redirect) {
                int fd = open(info->redirect_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd == -1) {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            // Execute second command
            execvp(cmd2[0], cmd2);
            perror("execvp");
            exit(EXIT_FAILURE);
        }

        // Parent process
        close(pipefd[0]);
        close(pipefd[1]);
        waitpid(pid1, NULL, 0);
        waitpid(pid2, NULL, 0);
        exit(EXIT_SUCCESS);
    } else {
        // Handle redirection if present
        if (info->has_redirect) {
            int fd = open(info->redirect_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        // Execute the command
        execvp(info->tokens[0], info->tokens);
        perror("execvp");
        exit(EXIT_FAILURE);
    }
}

