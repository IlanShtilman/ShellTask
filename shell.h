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
#define DELIMITERS " \t\r\n\a"

typedef struct parseInfo {
    char **tokens;    /* Array of command tokens */
    int size;         /* Number of tokens */
    int has_pipe;     /* Flag indicating if command contains pipe */
    int pipe_index;   /* Index of pipe in tokens array */
    int has_redirect; /* Flag indicating if command contains redirection */
    int redirect_index; /* Index of > in tokens array */
    char *redirect_file; /* File to redirect output to */
} parseInfo;

/* Functions*/
char* readline(char *prompt);
parseInfo* parse(char *cmdLine);
void free_parse_info(parseInfo *info);
void executeCommand(parseInfo *info);
int isBuiltInCommand(parseInfo *info);
int executeBuiltInCommand(parseInfo *info);
void print_tree(const char *path, int level);
void execute_tree(parseInfo *info);

#endif 