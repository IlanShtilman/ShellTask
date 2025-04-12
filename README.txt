Custom Shell Implementation
=======================

This is a custom shell implementation in C for Linux environment.

Compilation
----------
To compile the shell, use the following command:
gcc -o mysh main.c shell_functions.c

Running the Shell
---------------
After compilation, run the shell using:
./mysh

Supported Commands
----------------
The shell supports the following commands:

1. Directory Operations:
   - pwd: Show current working directory
   - cd [directory]: Change directory
   - tree [directory]: Display directory structure in tree format
   - ls, ls -l: List directory contents
   - rmdir [directory]: Remove empty directory

2. File Operations:
   - cat [file]: Display file contents
   - cat > [file]: Create new file (press Ctrl+D to save and exit)
   - cp [source] [destination]: Copy files
   - chmod [permissions] [file]: Change file permissions
     Examples: 
     - chmod 755 file.txt
     - chmod g+w file.txt

3. Text Processing:
   - grep [pattern] [file]: Search for pattern in file
   - grep -c [pattern] [file]: Count occurrences of pattern

4. Output Redirection:
   - command > file: Redirect output to file
   Example: ls -l > output.txt

5. Pipes:
   - command1 | command2: Pipe output of command1 to command2
   Example: ls -l | grep .txt

6. System Commands:
   - clear: Clear the screen
   - exit: Exit the shell

Examples
--------
1. List files and redirect to output:
   ls -l > output.txt

2. Search for files:
   ls -l | grep .txt

3. Change directory and show contents:
   cd /home
   ls -l

4. Create and modify files:
   cat > file.txt
   chmod 755 file.txt

5. View directory structure:
   tree
   tree /home

Notes
-----
- Use Ctrl+D to end input when using cat > file
- Use Ctrl+C to interrupt any running command
- The shell prompt is "mysh>"

Error Handling
-------------
The shell includes error handling for:
- Invalid commands
- File permission issues
- Directory access issues
- Invalid arguments
- Pipe and redirection errors 