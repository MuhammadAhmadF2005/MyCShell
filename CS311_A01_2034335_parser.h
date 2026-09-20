#ifndef PARSER_H
#define PARSER_H

#include <stddef.h>

#define MAX_ARGS 128
#define MAX_PIPE_COMMANDS 2
#define MAX_JOBS 32
#define MAX_TOKEN_LENGTH 1024

typedef struct {
    char *argv[MAX_ARGS];
    int argc;
    char *input_file;
    char *output_file;
    int append_output;
} Command;

typedef struct {
    Command commands[MAX_PIPE_COMMANDS];
    int command_count;
    int background;
} Job;

typedef struct {
    Job jobs[MAX_JOBS];
    int job_count;
} ParsedLine;

void parsed_line_init(ParsedLine *line);
void parsed_line_destroy(ParsedLine *line);
int parse_line(const char *input, ParsedLine *line, char *error, size_t error_size);

#endif
