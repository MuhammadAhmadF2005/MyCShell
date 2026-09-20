#ifndef PARSER_H
#define PARSER_H

#include <stddef.h>

#define MAX_ARGS 128
#define MAX_PIPE_COMMANDS 2
#define MAX_JOBS 32
#define MAX_TOKEN_LENGTH 1024

// Represents a single command with arguments and file redirections (<, >, >>)
typedef struct {
    char *argv[MAX_ARGS];
    int argc;
    char *input_file;
    char *output_file;
    int append_output;
} Command;

// Represents a job: either a single command or commands joined by a pipe (|)
typedef struct {
    Command commands[MAX_PIPE_COMMANDS];
    int command_count;
    int background;
} Job;

// Represents the full parsed input line containing one or more jobs (& separated)
typedef struct {
    Job jobs[MAX_JOBS];
    int job_count;
} ParsedLine;

// Initialize parsed structure
void parsed_line_init(ParsedLine *line);
// Free memory allocated for tokens and file paths
void parsed_line_destroy(ParsedLine *line);
// Parse a raw input string into structured commands and jobs
int parse_line(const char *input, ParsedLine *line, char *error, size_t error_size);

#endif

