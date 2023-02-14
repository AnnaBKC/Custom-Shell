# Custom Shell (smallsh)

This program is a custom shell implementation in C containig a subset of features of well-known shells, such as bash.
The program has the following functionalities:

- Provides a prompt for running commands
- Handles/ignores blank lines and comments (lines beginning with the # character)
- Provides expansion for the variable $$
- Executes 3 commands exit, cd, and status via code built into the shell
- Executes other commands by creating new processes using a function from the exec family of functions
- Supports input and output redirection
- Supports running commands in foreground and background processes
- Implements custom handlers for 2 signals, SIGINT and SIGTSTP


## Instructions

Compile:
```sh
gcc --std=gnu99 -o smallsh main.c
```
Execute:
```sh
./smallh
```

Alternately you can also compile/execute the code by using the 'make' command:
```sh
make
```
