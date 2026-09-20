#define _POSIX_C_SOURCE 200809L

#include "CS311_A01_2034335_builtins.h"
#include "CS311_A01_2034335_process.h"

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void print_builtin_usage(const char *command)
{
    if (strcmp(command, "cd") == 0) {
        fprintf(stderr, "usage: cd [directory]\n");
    } else if (strcmp(command, "pwd") == 0) {
        fprintf(stderr, "usage: pwd\n");
    } else if (strcmp(command, "ps") == 0) {
        fprintf(stderr, "usage: ps\n");
    } else if (strcmp(command, "kill") == 0) {
        fprintf(stderr, "usage: kill PID\n");
    } else if (strcmp(command, "exit") == 0) {
        fprintf(stderr, "usage: exit [status]\n");
    }
}

// Change current working directory 
static int builtin_cd(const Command *command)
{
    const char *directory;
    char oldcwd[PATH_MAX];
    char newcwd[PATH_MAX];

    if (command->argc > 2) {
        print_builtin_usage("cd");
        return -1;
    }

    if (getcwd(oldcwd, sizeof(oldcwd)) == NULL) {
        fprintf(stderr, "cd: cannot determine current directory: %s\n", strerror(errno));
        return -1;
    }

    if (command->argc == 1 || strcmp(command->argv[1], "~") == 0) {
        directory = getenv("HOME");
        if (directory == NULL || *directory == '\0') {
            fprintf(stderr, "cd: HOME is not set\n");
            return -1;
        }
    } else if (strcmp(command->argv[1], "-") == 0) {
        directory = getenv("OLDPWD");
        if (directory == NULL || *directory == '\0') {
            fprintf(stderr, "cd: OLDPWD is not set\n");
            return -1;
        }
    } else if (strncmp(command->argv[1], "~/", 2) == 0) {
        static char expanded[PATH_MAX];
        const char *home = getenv("HOME");
        if (home == NULL || *home == '\0') {
            fprintf(stderr, "cd: HOME is not set\n");
            return -1;
        }
        snprintf(expanded, sizeof(expanded), "%s/%s", home, command->argv[1] + 2);
        directory = expanded;
    } else {
        directory = command->argv[1];
    }

    if (chdir(directory) == -1) {
        fprintf(stderr, "cd: %s: %s\n", directory, strerror(errno));
        return -1;
    }

    if (getcwd(newcwd, sizeof(newcwd)) == NULL) {
        fprintf(stderr, "cd: warning: directory changed, but new path is unavailable: %s\n",
                strerror(errno));
        return 0;
    }

    if (setenv("OLDPWD", oldcwd, 1) == -1) {
        fprintf(stderr, "cd: warning: could not update OLDPWD: %s\n", strerror(errno));
    }
    if (setenv("PWD", newcwd, 1) == -1) {
        fprintf(stderr, "cd: warning: could not update PWD: %s\n", strerror(errno));
    }

    if (command->argc == 2 && strcmp(command->argv[1], "-") == 0) {
        puts(newcwd);
    }

    return 0;
}

// Print absolute path of current working directory (pwd)
static int builtin_pwd(const Command *command)
{
    char cwd[PATH_MAX];

    if (command->argc != 1) {
        print_builtin_usage("pwd");
        return -1;
    }

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("pwd: getcwd");
        return -1;
    }

    puts(cwd);
    return 0;
}

// Clean up dead background processes (ps)
static int builtin_ps(const Command *command)
{
    if (command->argc != 1) {
        print_builtin_usage("ps");
        return -1;
    }

    process_reap_finished();
    process_print();
    return 0;
}

static int parse_pid(const char *text, pid_t *pid)
{
    char *end = NULL;
    long value;

    if (text == NULL || *text == '\0') {
        return -1;
    }

    errno = 0;
    value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value <= 0 ||
        value > INT_MAX) {
        return -1;
    }

    *pid = (pid_t)value;
    return 0;
}

// Send SIGTERM to terminate a monitored background child process
static int builtin_kill(const Command *command)
{
    pid_t pid;

    if (command->argc != 2) {
        print_builtin_usage("kill");
        return -1;
    }

    if (parse_pid(command->argv[1], &pid) == -1) {
        fprintf(stderr, "kill: invalid PID: %s\n", command->argv[1]);
        return -1;
    }

    if (process_find(pid, NULL) == -1) {
        fprintf(stderr, "kill: PID %ld is not managed by mysh\n", (long)pid);
        return -1;
    }

    if (kill(pid, SIGTERM) == -1) {
        fprintf(stderr, "kill: %ld: %s\n", (long)pid, strerror(errno));
        if (errno == ESRCH) {
            (void)process_remove(pid);
        }
        return -1;
    }

    (void)process_remove(pid);
    return 0;
}

// Exit the shell cleanly with optional status code
static int builtin_exit(const Command *command)
{
    int exit_code = 0;

    if (command->argc > 2) {
        print_builtin_usage("exit");
        return -1;
    }

    if (command->argc == 2) {
        char *end = NULL;
        long value;

        errno = 0;
        value = strtol(command->argv[1], &end, 10);
        if (errno != 0 || end == command->argv[1] || *end != '\0') {
            fprintf(stderr, "exit: %s: numeric argument required\n", command->argv[1]);
            exit_code = 2;
        } else {
            exit_code = (int)value;
        }
    }

    process_terminate_all();
    exit(exit_code);
    return 0;
}

// Check if command is implemented as an internal shell builtin
int is_builtin(const Command *command)
{
    if (command == NULL || command->argc == 0) {
        return 0;
    }

    return strcmp(command->argv[0], "cd") == 0 ||
           strcmp(command->argv[0], "pwd") == 0 ||
           strcmp(command->argv[0], "ps") == 0 ||
           strcmp(command->argv[0], "kill") == 0 ||
           strcmp(command->argv[0], "exit") == 0;
}

// Dispatch builtin command to its corresponding handler function
int execute_builtin(const Command *command)
{
    if (command == NULL || command->argc == 0) {
        return -1;
    }

    if (strcmp(command->argv[0], "cd") == 0) {
        return builtin_cd(command);
    }
    if (strcmp(command->argv[0], "pwd") == 0) {
        return builtin_pwd(command);
    }
    if (strcmp(command->argv[0], "ps") == 0) {
        return builtin_ps(command);
    }
    if (strcmp(command->argv[0], "kill") == 0) {
        return builtin_kill(command);
    }
    if (strcmp(command->argv[0], "exit") == 0) {
        return builtin_exit(command);
    }

    return -1;
}
