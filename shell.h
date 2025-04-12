#ifndef SHELL_H
#define SHELL_H

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
#include <time.h>
#include <regex.h>

/* Constants */
#define MAX_INPUT_SIZE 1024
#define MAX_ARGS 64
#define MAX_PIPES 10
#define DELIMITERS " \t\r\n\a"

/* Structure to hold parsed command information */
typedef struct {
    char **args;           /* Command and its arguments */
    int arg_count;         /* Number of arguments */
    int has_pipe;          /* Flag for pipe */
    int pipe_count;        /* Number of pipes */
    char **pipe_commands[MAX_PIPES]; /* Array of command arrays for each pipe */
    int pipe_arg_counts[MAX_PIPES];  /* Number of arguments for each pipe command */
    int has_output_redir;  /* Flag for output redirection */
    char *output_file;     /* Output redirection filename */
    int has_input_redir;   /* Flag for input redirection */
    char *input_file;      /* Input redirection filename */
    int has_append_redir;  /* Flag for append redirection */
    char *append_file;     /* Append redirection filename */
} parseInfo;

/* Function declarations */

/* Read a line from stdin with a prompt */
char* readline(char *prompt);

/* Parse the command line into arguments */
parseInfo* parse(char *cmdLine);

/* Execute the command */
void executeCommand(parseInfo *info);

/* Check if command is a built-in command */
int is_builtin_command(char *command);

/* Execute built-in commands */
void execute_builtin_command(parseInfo *info);

/* Free memory allocated for parseInfo */
void free_parse_info(parseInfo *info);

/* Built-in command functions */
void shell_cd(char **args);
void shell_exit();
void shell_pwd();
void shell_clear();
void shell_rmdir(char **args);
void shell_ls(char **args);
void shell_grep(char **args);

/* Special command functions */
void execute_pipe(parseInfo *info);
void execute_multi_pipe(parseInfo *info);
void execute_redirection(parseInfo *info);
void execute_input_redirection(parseInfo *info);
void execute_append_redirection(parseInfo *info);
void print_tree(char *path, int level);

#endif /* SHELL_H */