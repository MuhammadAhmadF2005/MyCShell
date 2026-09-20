#define _POSIX_C_SOURCE 200809L

#include "process.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static Process process_table[MAX_PROCESSES];
static size_t process_count = 0;

// Initialize process tracking table and reset count
void process_init(void)
{
    process_count = 0;
    memset(process_table, 0, sizeof(process_table));
}

// Register a background process in our tracking table
int process_add(pid_t pid, const char *name)
{
    size_t i;

    if (pid <= 0 || name == NULL || *name == '\0') {
        return -1;
    }

    if (process_count >= MAX_PROCESSES) {
        return -1;
    }

    for (i = 0; i < process_count; ++i) {
        if (process_table[i].pid == pid) {
            return 0;
        }
    }

    process_table[process_count].pid = pid;
    snprintf(process_table[process_count].name,
             sizeof(process_table[process_count].name), "%s", name);
    ++process_count;
    return 0;
}

// Remove process from table by shifting remaining entries left
int process_remove(pid_t pid)
{
    size_t i;

    for (i = 0; i < process_count; ++i) {
        if (process_table[i].pid == pid) {
            if (i + 1 < process_count) {
                memmove(&process_table[i], &process_table[i + 1],
                        (process_count - i - 1) * sizeof(process_table[0]));
            }
            --process_count;
            return 0;
        }
    }

    return -1;
}

int process_find(pid_t pid, Process *out)
{
    size_t i;

    for (i = 0; i < process_count; ++i) {
        if (process_table[i].pid == pid) {
            if (out != NULL) {
                *out = process_table[i];
            }
            return 0;
        }
    }

    return -1;
}

// Display PID and command name for all active background jobs
void process_print(void)
{
    size_t i;

    printf("%-10s %s\n", "PID", "COMMAND");
    for (i = 0; i < process_count; ++i) {
        printf("%-10ld %s\n", (long)process_table[i].pid,
               process_table[i].name);
    }
}

// Non-blocking waitpid check to clean up finished background processes
void process_reap_finished(void)
{
    int status;

    while (1) {
        pid_t pid = waitpid(-1, &status, WNOHANG);

        if (pid > 0) {
            if (process_find(pid, NULL) == 0) {
                (void)process_remove(pid);
            }
            continue;
        }

        if (pid == 0) {
            return;
        }

        if (errno == EINTR) {
            continue;
        }

        if (errno == ECHILD) {
            return;
        }

        perror("waitpid");
        return;
    }
}

// Send SIGTERM and wait for all background children before exiting shell
void process_terminate_all(void)
{
    size_t i;

    for (i = 0; i < process_count; ++i) {
        if (kill(process_table[i].pid, SIGTERM) == -1 && errno != ESRCH) {
            perror("kill");
        }
    }

    for (i = 0; i < process_count; ++i) {
        int status;
        pid_t result;

        do {
            result = waitpid(process_table[i].pid, &status, 0);
        } while (result == -1 && errno == EINTR);
    }

    process_count = 0;
}
