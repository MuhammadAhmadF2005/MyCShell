#ifndef BUILTINS_H
#define BUILTINS_H

#include "parser.h"

// Check if a command is an internal shell builtin (cd, pwd, ps, kill)
int is_builtin(const Command *command);

// Execute the builtin directly in the shell process ..//
int execute_builtin(const Command *command);

#endif

