#define _POSIX_C_SOURCE 200809L

#include "CS311_A01_2034335_parser.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_error(char *error, size_t error_size, const char *message)
{
    if (error != NULL && error_size > 0) {
        snprintf(error, error_size, "%s", message);
    }
}

void parsed_line_init(ParsedLine *line)
{
    if (line == NULL) {
        return;
    }
    memset(line, 0, sizeof(*line));
}

void parsed_line_destroy(ParsedLine *line)
{
    int i;
    int j;

    if (line == NULL) {
        return;
    }

    for (i = 0; i < line->job_count; ++i) {
        for (j = 0; j < line->jobs[i].command_count; ++j) {
            int k;
            Command *command = &line->jobs[i].commands[j];

            for (k = 0; k < command->argc; ++k) {
                free(command->argv[k]);
                command->argv[k] = NULL;
            }
            free(command->input_file);
            free(command->output_file);
            command->input_file = NULL;
            command->output_file = NULL;
        }
    }

    line->job_count = 0;
}

static int add_argument(Command *command, const char *token,
                        char *error, size_t error_size)
{
    char *copy;

    if (command->argc >= MAX_ARGS - 1) {
        set_error(error, error_size, "too many command arguments");
        return -1;
    }

    copy = strdup(token);
    if (copy == NULL) {
        set_error(error, error_size, "out of memory");
        return -1;
    }

    command->argv[command->argc++] = copy;
    command->argv[command->argc] = NULL;
    return 0;
}

static int set_filename(char **destination, const char *token,
                        char *error, size_t error_size)
{
    char *copy;

    if (*destination != NULL) {
        set_error(error, error_size, "duplicate redirection operator");
        return -1;
    }

    copy = strdup(token);
    if (copy == NULL) {
        set_error(error, error_size, "out of memory");
        return -1;
    }
    *destination = copy;
    return 0;
}

int parse_line(const char *input, ParsedLine *line, char *error, size_t error_size)
{
    size_t i = 0;
    Job *job;
    Command *command;
    int expect_input_file = 0;
    int expect_output_file = 0;
    int saw_token = 0;

    if (error != NULL && error_size > 0) {
        error[0] = '\0';
    }

    if (input == NULL || line == NULL) {
        set_error(error, error_size, "invalid parser input");
        return -1;
    }

    parsed_line_init(line);
    line->job_count = 1;
    job = &line->jobs[0];
    job->command_count = 1;
    command = &job->commands[0];

    while (input[i] != '\0') {
        char token[MAX_TOKEN_LENGTH];
        size_t token_len = 0;

        while (isspace((unsigned char)input[i])) {
            ++i;
        }
        if (input[i] == '\0') {
            break;
        }

        if (input[i] == '>') {
            if (expect_input_file || expect_output_file || command->argc == 0) {
                set_error(error, error_size, "invalid output redirection placement");
                parsed_line_destroy(line);
                return -1;
            }

            if (input[i + 1] == '>') {
                command->append_output = 1;
                i += 2;
            } else {
                command->append_output = 0;
                i += 1;
            }
            expect_output_file = 1;
            continue;
        }

        if (input[i] == '<') {
            if (expect_input_file || expect_output_file || command->argc == 0) {
                set_error(error, error_size, "invalid input redirection placement");
                parsed_line_destroy(line);
                return -1;
            }
            i += 1;
            expect_input_file = 1;
            continue;
        }

        if (input[i] == '|') {
            if (expect_input_file || expect_output_file || command->argc == 0) {
                set_error(error, error_size, "invalid pipe placement");
                parsed_line_destroy(line);
                return -1;
            }

            if (job->command_count >= MAX_PIPE_COMMANDS) {
                set_error(error, error_size, "only two pipeline commands are supported");
                parsed_line_destroy(line);
                return -1;
            }

            ++i;
            command = &job->commands[job->command_count++];
            saw_token = 0;
            continue;
        }

        if (input[i] == '&') {
            if (expect_input_file || expect_output_file || command->argc == 0) {
                set_error(error, error_size, "invalid '&' placement");
                parsed_line_destroy(line);
                return -1;
            }

            job->background = 1;
            ++i;
            while (isspace((unsigned char)input[i])) {
                ++i;
            }

            if (input[i] == '\0') {
                continue;
            }

            if (line->job_count >= MAX_JOBS) {
                set_error(error, error_size, "too many commands on one line");
                parsed_line_destroy(line);
                return -1;
            }

            job = &line->jobs[line->job_count++];
            job->command_count = 1;
            command = &job->commands[0];
            saw_token = 0;
            continue;
        }

        while (input[i] != '\0' && !isspace((unsigned char)input[i]) &&
               input[i] != '>' && input[i] != '<' && input[i] != '|' &&
               input[i] != '&') {
            if (token_len + 1 >= sizeof(token)) {
                set_error(error, error_size, "token is too long");
                parsed_line_destroy(line);
                return -1;
            }
            token[token_len++] = input[i++];
        }
        token[token_len] = '\0';

        if (token_len == 0) {
            continue;
        }

        saw_token = 1;
        if (expect_input_file) {
            if (set_filename(&command->input_file, token, error, error_size) == -1) {
                parsed_line_destroy(line);
                return -1;
            }
            expect_input_file = 0;
        } else if (expect_output_file) {
            if (set_filename(&command->output_file, token, error, error_size) == -1) {
                parsed_line_destroy(line);
                return -1;
            }
            expect_output_file = 0;
        } else if (add_argument(command, token, error, error_size) == -1) {
            parsed_line_destroy(line);
            return -1;
        }
    }

    if (expect_input_file || expect_output_file) {
        set_error(error, error_size, "redirection operator requires a filename");
        parsed_line_destroy(line);
        return -1;
    }

    if (!saw_token && line->jobs[0].commands[0].argc == 0) {
        parsed_line_destroy(line);
        return 0;
    }

    for (i = 0; i < (size_t)line->job_count; ++i) {
        size_t j;
        Job *current_job = &line->jobs[i];

        for (j = 0; j < (size_t)current_job->command_count; ++j) {
            if (current_job->commands[j].argc == 0) {
                set_error(error, error_size, "empty command in command or pipeline");
                parsed_line_destroy(line);
                return -1;
            }
        }
    }

    return 1;
}
