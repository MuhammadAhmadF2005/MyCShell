#ifndef BUILTINS_H
#define BUILTINS_H

#include "CS311_A01_2034335_parser.h"

// Returns 1 if the command is a shell builtin, 0 otherwise. 
int is_builtin(const Command *command);

// Executes a standalone builtin in the shell process. 
int execute_builtin(const Command *command);

#endif
