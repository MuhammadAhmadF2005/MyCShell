#ifndef SHELL_H
#define SHELL_H

#include "CS311_A01_2034335_parser.h"

// Execute all jobs on the parsed command line (builtins, single commands, pipelines)
int execute_parsed_line(const ParsedLine *line);

#endif

