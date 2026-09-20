#define _POSIX_C_SOURCE 200809L

#include "CS311_A01_2034335_builtins.h"
#include "CS311_A01_2034335_linenoise.h"
#include "CS311_A01_2034335_parser.h"
#include "CS311_A01_2034335_process.h"
#include "CS311_A01_2034335_shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PROMPT_MAX 512
#define ERROR_MAX 256

static void build_prompt(char *prompt, size_t prompt_size)
{
    char cwd[256];

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        (void)snprintf(prompt, prompt_size, "mysh> ");
        return;
    }

    (void)snprintf(prompt, prompt_size, "mysh:%s$ ", cwd);
}

int main(int argc, char *argv[], char *envp[])
{
    char prompt[PROMPT_MAX];
    char error[ERROR_MAX];
    char *line;

    (void)argc;
    (void)argv;
    (void)envp;

    process_init();
    (void)linenoiseHistorySetMaxLen(100);

    while (1) {
        ParsedLine parsed;
        int parse_status;
        int exec_status;

        process_reap_finished();
        build_prompt(prompt, sizeof(prompt));

        line = linenoise(prompt);
        if (line == NULL) {
            break;
        }

        if (line[0] == '\0') {
            linenoiseFree(line);
            continue;
        }

        if (linenoiseHistoryAdd(line) == -1) {
            fprintf(stderr, "mysh: warning: could not save command history\n");
        }

        parsed_line_init(&parsed);
        parse_status = parse_line(line, &parsed, error, sizeof(error));
        linenoiseFree(line);

        if (parse_status == -1) {
            fprintf(stderr, "mysh: %s\n", error);
            continue;
        }
        if (parse_status == 0) {
            continue;
        }

        exec_status = execute_parsed_line(&parsed);
        if (exec_status == -1) {
            /* The called function has already printed a helpful error. */
        }

        parsed_line_destroy(&parsed);
    }

    process_terminate_all();
    return EXIT_SUCCESS;
}
