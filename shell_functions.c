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

/* Read a line from stdin with a prompt */
char* readline(char *prompt) {
    char *line = NULL;
    size_t bufsize = 0;
    
    printf("%s", prompt);
    fflush(stdout);
    
    ssize_t chars_read = getline(&line, &bufsize, stdin);
    
    if (chars_read == -1) {
        // Handle EOF (Ctrl+D)
        if (feof(stdin)) {
            printf("\n");
            free(line);
            return NULL;
        }
        perror("readline");
        exit(EXIT_FAILURE);
    }
    
    return line;
}

//Function for parsing the command line - first we initilize the parseInfo struct, then we parse the command line and after that checking for pipe and output redirection   
parseInfo* parse(char *cmdLine){
    parseInfo *info = (parseInfo *)malloc(sizeof(parseInfo));
    if(info == NULL){
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    info->args = (char **)malloc(MAX_ARGS * sizeof(char *));
    if(!info->args){
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    
    // Initialize pipe command arrays
    for(int i = 0; i < MAX_PIPES; i++) {
        info->pipe_commands[i] = (char **)malloc(MAX_ARGS * sizeof(char *));
        if(!info->pipe_commands[i]){
            perror("malloc");
            exit(EXIT_FAILURE);
        }
        info->pipe_arg_counts[i] = 0;
    }
    
    info->arg_count = 0;
    info->has_pipe = 0;
    info->pipe_count = 0;
    info->has_output_redir = 0;
    info->output_file = NULL;
    info->has_input_redir = 0;
    info->input_file = NULL;
    info->has_append_redir = 0;
    info->append_file = NULL;

    char *token;
    int position = 0;
    int current_pipe = 0;
    int output_redirect_found = 0;
    int input_redirect_found = 0;
    int append_redirect_found = 0;
    
    // Parse the command line
    token = strtok(cmdLine, DELIMITERS);
    while (token != NULL) {
        // Check for pipe
        if (strcmp(token, "|") == 0) {
            info->has_pipe = 1;
            info->pipe_count++;
            
            // Terminate current command
            if (current_pipe == 0) {
                info->args[position] = NULL;
                info->arg_count = position;
            } else {
                info->pipe_commands[current_pipe-1][position] = NULL;
                info->pipe_arg_counts[current_pipe-1] = position;
            }
            
            // Reset position for next command
            position = 0;
            current_pipe++;
            
            // Check if we've exceeded the maximum number of pipes
            if (current_pipe >= MAX_PIPES) {
                fprintf(stderr, "Too many pipes. Maximum allowed: %d\n", MAX_PIPES);
                free_parse_info(info);
                return NULL;
            }
        }
        // Check for output redirection
        else if (strcmp(token, ">") == 0) {
            output_redirect_found = 1;
            info->has_output_redir = 1;
            
            // Terminate current command
            if (current_pipe == 0) {
                info->args[position] = NULL;
                info->arg_count = position;
            } else {
                info->pipe_commands[current_pipe-1][position] = NULL;
                info->pipe_arg_counts[current_pipe-1] = position;
            }
        }
        // Check for input redirection
        else if (strcmp(token, "<") == 0) {
            input_redirect_found = 1;
            info->has_input_redir = 1;
            
            // Terminate current command
            if (current_pipe == 0) {
                info->args[position] = NULL;
                info->arg_count = position;
            } else {
                info->pipe_commands[current_pipe-1][position] = NULL;
                info->pipe_arg_counts[current_pipe-1] = position;
            }
        }
        // Check for append redirection
        else if (strcmp(token, ">>") == 0) {
            append_redirect_found = 1;
            info->has_append_redir = 1;
            
            // Terminate current command
            if (current_pipe == 0) {
                info->args[position] = NULL;
                info->arg_count = position;
            } else {
                info->pipe_commands[current_pipe-1][position] = NULL;
                info->pipe_arg_counts[current_pipe-1] = position;
            }
        }
        // Get output file name
        else if (output_redirect_found) {
            info->output_file = strdup(token);
            output_redirect_found = 0;
        }
        // Get input file name
        else if (input_redirect_found) {
            info->input_file = strdup(token);
            input_redirect_found = 0;
        }
        // Get append file name
        else if (append_redirect_found) {
            info->append_file = strdup(token);
            append_redirect_found = 0;
        }
        // Add argument to appropriate list
        else {
            if (current_pipe == 0) {
                info->args[position] = token;
            } else {
                info->pipe_commands[current_pipe-1][position] = token;
            }
            position++;
        }
        
        token = strtok(NULL, DELIMITERS);
    }
    
    // Terminate the last command
    if (current_pipe == 0) {
        info->args[position] = NULL;
        info->arg_count = position;
    } else {
        info->pipe_commands[current_pipe-1][position] = NULL;
        info->pipe_arg_counts[current_pipe-1] = position;
    }
    
    return info;
}

void executeCommand(parseInfo *info){
    if(info->has_pipe){
        if(info->pipe_count > 1) {
            execute_multi_pipe(info);
        } else {
            execute_pipe(info);
        }
        return;
    }
    else if(info->has_output_redir){
        execute_redirection(info);
        return;
    }
    else if(info->has_input_redir){
        execute_input_redirection(info);
        return;
    }
    else if(info->has_append_redir){
        execute_append_redirection(info);
        return;
    }
    // Special case for tree command
    if (info->args[0] && strcmp(info->args[0], "tree") == 0) {
        char *path = ".";
        if (info->args[1] != NULL) {
            path = info->args[1];
        }
        print_tree(path, 0);
        return;
    }
    
    // Execute regular command
    if (execvp(info->args[0], info->args) == -1) {
        perror("Command execution failed");
        exit(EXIT_FAILURE);
    }
}
    
//Check if command is a built-in command
int is_builtin_command(char *command){
    if(command == NULL){
        return 0;
    }
    if(strcmp(command, "cd") == 0 || strcmp(command, "exit") == 0 || 
       strcmp(command, "pwd") == 0 || strcmp(command, "clear") == 0 || 
       strcmp(command, "rmdir") == 0 || strcmp(command, "ls") == 0 ||
       strcmp(command, "grep") == 0){
        return 1;
    }
    return 0;
}

void execute_builtin_command(parseInfo *info){
   char *command = info->args[0];
   if(strcmp(command, "cd") == 0){
    shell_cd(info->args);
   }
   else if(strcmp(command, "exit") == 0){
    shell_exit();
   }
   else if(strcmp(command, "pwd") == 0){
    shell_pwd();
   }
   else if(strcmp(command, "clear") == 0){
    shell_clear();
   }
   else if(strcmp(command, "rmdir") == 0){
    shell_rmdir(info->args);
   }
   else if(strcmp(command, "ls") == 0){
    shell_ls(info->args);
   }
   else if(strcmp(command, "grep") == 0){
    shell_grep(info->args);
   }
}

/* Free memory allocated for parseInfo */
void free_parse_info(parseInfo *info) {
    if (info) {
        free(info->args);
        for(int i = 0; i < MAX_PIPES; i++) {
            free(info->pipe_commands[i]);
        }
        if (info->output_file) {
            free(info->output_file);
        }
        if (info->input_file) {
            free(info->input_file);
        }
        if (info->append_file) {
            free(info->append_file);
        }
        free(info);
    }
}

/* Built-in command: cd */
void shell_cd(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "cd: expected argument\n");
    } else {
        if (chdir(args[1]) != 0) {
            perror("cd");
        }
    }
}

/* Built-in command: exit */
void shell_exit() {
    exit(0);
}

/* Built-in command: pwd */
void shell_pwd() {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("pwd");
    }
}

/* Built-in command: clear */
void shell_clear() {
    // ANSI escape sequence to clear the terminal
    printf("\033[H\033[J");
}

/* Built-in command: rmdir */
void shell_rmdir(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "rmdir: expected argument\n");
    } else {
        if (rmdir(args[1]) != 0) {
            perror("rmdir");
        }
    }
}

/* Built-in command: ls */
void shell_ls(char **args) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    char path[1024];
    int show_details = 0;
    int show_hidden = 0;
    char *target_dir = ".";
    
    // Parse options
    int i = 1;
    while (args[i] != NULL && args[i][0] == '-') {
        int j = 1;
        while (args[i][j] != '\0') {
            if (args[i][j] == 'l') {
                show_details = 1;
            } else if (args[i][j] == 'a') {
                show_hidden = 1;
            }
            j++;
        }
        i++;
    }
    
    // Get target directory
    if (args[i] != NULL) {
        target_dir = args[i];
    }
    
    // Open directory
    if (!(dir = opendir(target_dir))) {
        perror("opendir");
        return;
    }
    
    // If detailed listing is requested, print header
    if (show_details) {
        printf("total %d\n", 0); // Placeholder for total blocks
    }
    
    // Read directory entries
    while ((entry = readdir(dir)) != NULL) {
        // Skip hidden files if not requested
        if (!show_hidden && entry->d_name[0] == '.') {
            continue;
        }
        
        // Build full path for stat
        snprintf(path, sizeof(path), "%s/%s", target_dir, entry->d_name);
        
        if (show_details) {
            // Get file information
            if (lstat(path, &st) == -1) {
                perror("lstat");
                continue;
            }
            
            // Print permissions
            printf("%c", S_ISDIR(st.st_mode) ? 'd' : 
                         S_ISLNK(st.st_mode) ? 'l' : '-');
            printf("%c%c%c", st.st_mode & S_IRUSR ? 'r' : '-',
                             st.st_mode & S_IWUSR ? 'w' : '-',
                             st.st_mode & S_IXUSR ? 'x' : '-');
            printf("%c%c%c", st.st_mode & S_IRGRP ? 'r' : '-',
                             st.st_mode & S_IWGRP ? 'w' : '-',
                             st.st_mode & S_IXGRP ? 'x' : '-');
            printf("%c%c%c ", st.st_mode & S_IROTH ? 'r' : '-',
                              st.st_mode & S_IWOTH ? 'w' : '-',
                              st.st_mode & S_IXOTH ? 'x' : '-');
            
            // Print number of links
            printf("%ld ", (long)st.st_nlink);
            
            // Print owner and group
            struct passwd *pw = getpwuid(st.st_uid);
            struct group *gr = getgrgid(st.st_gid);
            printf("%s %s ", pw ? pw->pw_name : "unknown", gr ? gr->gr_name : "unknown");
            
            // Print size
            printf("%ld ", (long)st.st_size);
            
            // Print modification time
            char time_str[20];
            strftime(time_str, sizeof(time_str), "%b %d %H:%M", localtime(&st.st_mtime));
            printf("%s ", time_str);
        }
        
        // Print file/directory name
        printf("%s\n", entry->d_name);
    }
    
    closedir(dir);
}

/* Execute command with pipe */
void execute_pipe(parseInfo *info) {
    int pipefd[2];
    pid_t pid1, pid2;
    
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    
    // First child process
    pid1 = fork();
    if (pid1 == 0) {
        // Child 1: Sets up the pipe and executes first command
        close(pipefd[0]); // Close read end
        dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to pipe
        close(pipefd[1]);
        
        if (execvp(info->args[0], info->args) == -1) {
            perror("Command execution failed");
            exit(EXIT_FAILURE);
        }
    } else if (pid1 < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    
    // Second child process
    pid2 = fork();
    if (pid2 == 0) {
        // Child 2: Gets input from the pipe and executes second command
        close(pipefd[1]); // Close write end
        dup2(pipefd[0], STDIN_FILENO); // Redirect stdin from pipe
        close(pipefd[0]);
        
        if (execvp(info->pipe_commands[0][0], info->pipe_commands[0]) == -1) {
            perror("Command execution failed");
            exit(EXIT_FAILURE);
        }
    } else if (pid2 < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    
    // Parent process: Close both ends of pipe and wait for children
    close(pipefd[0]);
    close(pipefd[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
    
    // Exit to prevent further execution in the parent's context
    exit(EXIT_SUCCESS);
}

/* Execute command with output redirection */
void execute_redirection(parseInfo *info) {
    int fd = open(info->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }
    
    // Redirect stdout to file
    dup2(fd, STDOUT_FILENO);
    close(fd);
    
    // Execute the command
    if (execvp(info->args[0], info->args) == -1) {
        perror("Command execution failed");
        exit(EXIT_FAILURE);
    }
}

/* Execute command with input redirection */
void execute_input_redirection(parseInfo *info) {
    int fd = open(info->input_file, O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }
    
    // Redirect stdin from file
    dup2(fd, STDIN_FILENO);
    close(fd);
    
    // Execute the command
    if (execvp(info->args[0], info->args) == -1) {
        perror("Command execution failed");
        exit(EXIT_FAILURE);
    }
}

/* Execute command with append redirection */
void execute_append_redirection(parseInfo *info) {
    int fd = open(info->append_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }
    
    // Redirect stdout to file (append mode)
    dup2(fd, STDOUT_FILENO);
    close(fd);
    
    // Execute the command
    if (execvp(info->args[0], info->args) == -1) {
        perror("Command execution failed");
        exit(EXIT_FAILURE);
    }
}

/* Print directory tree */
void print_tree(char *path, int level) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    char new_path[1024];
    
    // Open directory
    if (!(dir = opendir(path))) {
        perror("opendir");
        return;
    }
    
    // Read directory entries
    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".." directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Build full path for stat
        snprintf(new_path, sizeof(new_path), "%s/%s", path, entry->d_name);
        
        // Get file information
        if (lstat(new_path, &st) == -1) {
            perror("lstat");
            continue;
        }
        
        // Print indentation
        for (int i = 0; i < level; i++) {
            printf("│   ");
        }
        
        // Print file/directory name with appropriate symbol
        if (S_ISLNK(st.st_mode)) {
            // Symbolic link
            char link_target[1024];
            ssize_t len = readlink(new_path, link_target, sizeof(link_target) - 1);
            if (len != -1) {
                link_target[len] = '\0';
                printf("├── %s -> %s\n", entry->d_name, link_target);
            } else {
                printf("├── %s -> [broken link]\n", entry->d_name);
            }
        } else if (S_ISDIR(st.st_mode)) {
            // Directory
            printf("├── %s/\n", entry->d_name);
            
            // Recursively print subdirectories
            print_tree(new_path, level + 1);
        } else {
            // Regular file
            printf("├── %s\n", entry->d_name);
        }
    }
    
    closedir(dir);
}

/* Execute command with multiple pipes */
void execute_multi_pipe(parseInfo *info) {
    int num_pipes = info->pipe_count;
    int pipefd[MAX_PIPES][2];
    pid_t pids[MAX_PIPES + 1];
    
    // Create all the pipes
    for (int i = 0; i < num_pipes; i++) {
        if (pipe(pipefd[i]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }
    
    // Execute each command in the pipeline
    for (int i = 0; i <= num_pipes; i++) {
        pids[i] = fork();
        
        if (pids[i] == 0) {
            // Child process
            
            // Set up input pipe (except for first command)
            if (i > 0) {
                close(pipefd[i-1][1]); // Close write end
                dup2(pipefd[i-1][0], STDIN_FILENO); // Redirect stdin from pipe
                close(pipefd[i-1][0]);
            }
            
            // Set up output pipe (except for last command)
            if (i < num_pipes) {
                close(pipefd[i][0]); // Close read end
                dup2(pipefd[i][1], STDOUT_FILENO); // Redirect stdout to pipe
                close(pipefd[i][1]);
            }
            
            // Execute the command
            if (i == 0) {
                // First command
                if (execvp(info->args[0], info->args) == -1) {
                    perror("Command execution failed");
                    exit(EXIT_FAILURE);
                }
            } else {
                // Subsequent commands
                if (execvp(info->pipe_commands[i-1][0], info->pipe_commands[i-1]) == -1) {
                    perror("Command execution failed");
                    exit(EXIT_FAILURE);
                }
            }
        } else if (pids[i] < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
    }
    
    // Parent process: Close all pipe file descriptors
    for (int i = 0; i < num_pipes; i++) {
        close(pipefd[i][0]);
        close(pipefd[i][1]);
    }
    
    // Wait for all child processes to complete
    for (int i = 0; i <= num_pipes; i++) {
        waitpid(pids[i], NULL, 0);
    }
    
    // Exit to prevent further execution in the parent's context
    exit(EXIT_SUCCESS);
}

/* Built-in command: grep */
void shell_grep(char **args) {
    regex_t regex;
    char *pattern = NULL;
    char *filename = NULL;
    int case_sensitive = 1;
    int line_numbers = 0;
    int invert_match = 0;
    FILE *file;
    char line[MAX_INPUT_SIZE];
    int line_count = 0;
    int match_count = 0;
    int i = 1;
    
    // Parse options
    while (args[i] != NULL && args[i][0] == '-') {
        int j = 1;
        while (args[i][j] != '\0') {
            switch (args[i][j]) {
                case 'i':
                    case_sensitive = 0;
                    break;
                case 'n':
                    line_numbers = 1;
                    break;
                case 'v':
                    invert_match = 1;
                    break;
                default:
                    fprintf(stderr, "grep: invalid option -- '%c'\n", args[i][j]);
                    return;
            }
            j++;
        }
        i++;
    }
    
    // Get pattern and filename
    if (args[i] != NULL) {
        pattern = args[i++];
        if (args[i] != NULL) {
            filename = args[i];
        } else {
            fprintf(stderr, "grep: no input file\n");
            return;
        }
    } else {
        fprintf(stderr, "grep: no pattern\n");
        return;
    }
    
    // Compile regex pattern
    int regex_flags = REG_EXTENDED;
    if (!case_sensitive) {
        regex_flags |= REG_ICASE;
    }
    
    if (regcomp(&regex, pattern, regex_flags) != 0) {
        fprintf(stderr, "grep: invalid regular expression\n");
        return;
    }
    
    // Open file
    file = fopen(filename, "r");
    if (file == NULL) {
        perror("grep");
        regfree(&regex);
        return;
    }
    
    // Process each line
    while (fgets(line, sizeof(line), file) != NULL) {
        line_count++;
        int match = (regexec(&regex, line, 0, NULL, 0) == 0);
        
        if (match != invert_match) {
            match_count++;
            if (line_numbers) {
                printf("%d:", line_count);
            }
            printf("%s", line);
            if (line[strlen(line)-1] != '\n') {
                printf("\n");
            }
        }
    }
    
    // Clean up
    fclose(file);
    regfree(&regex);
}



