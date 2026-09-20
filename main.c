#define _POSIX_C_SOURCE 200809L

#include "builtins.h"
#include "linenoise.h"

#include "parser.h"
#include "process.h"
#include "shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PROMPT_MAX 512
#define ERROR_MAX 256

// Build prompt displaying the current working directory
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

    // Initialize process table and configure command history capacity
    process_init();
    (void)linenoiseHistorySetMaxLen(100);

    // Only print the welcome banner in interactive (terminal) mode
    if (isatty(STDIN_FILENO)) {
        printf("\n");
        printf("  ╔══════════════════════════════════════════╗\n");
        printf("  ║   __  ____  ______  __  __              ║\n");
        printf("  ║  /  |/  \\ \\/ / ___||  \\/  |             ║\n");
        printf("  ║ / /|_/ /\\  /\\___ \\| |\\/| |             ║\n");
        printf("  ║/ /  / /  / / ___) | |  | |             ║\n");
        printf("  ║/_/  /_/  /_/ |____/|_|  |_|  v1.0      ║\n");
        printf("  ║                                          ║\n");
        printf("  ║   MyCSHell  —  Greetings!! :)           ║\n");
        printf("  ║   Type a command to get started.         ║\n");
        printf("  ║   Use 'Ctrl + d' to quit.                    ║\n");
        printf("  ╚══════════════════════════════════════════╝\n");
        printf("\n");
    }

    // Main REPL loop
    while (1) {
        ParsedLine parsed;
        int parse_status;
        int exec_status;

        // Clean up finished background children before rendering next prompt
        process_reap_finished();
        build_prompt(prompt, sizeof(prompt));

        // Read user input interactively using linenoise
        line = linenoise(prompt);
        if (line == NULL) {
            break;
        }

        if (line[0] == '\0') {
            linenoiseFree(line);
            continue;
        }

        // Save non-empty command line into history
        if (linenoiseHistoryAdd(line) == -1) {
            fprintf(stderr, "mysh: warning: could not save command history\n");
        }

        // Parse input line into jobs and commands
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

        // Run the commands and free allocated parse trees
        exec_status = execute_parsed_line(&parsed);
        if (exec_status == -1) {
            /* The called function has already printed a helpful error. */
        }

        parsed_line_destroy(&parsed);
    }

    // Terminate any remaining background processes before exit
    process_terminate_all();
    return EXIT_SUCCESS;
}

