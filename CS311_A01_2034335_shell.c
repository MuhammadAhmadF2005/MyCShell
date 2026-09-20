#define _POSIX_C_SOURCE 200809L

#include "CS311_A01_2034335_shell.h"
#include "CS311_A01_2034335_builtins.h"
#include "CS311_A01_2034335_process.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int open_input(const char *filename)
{
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "mysh: cannot open '%s' for input: %s\n",
                filename, strerror(errno));
    }
    return fd;
}

static int open_output(const char *filename, int append)
{
    int flags = O_WRONLY | O_CREAT;
    int fd;

    flags |= append ? O_APPEND : O_TRUNC;
    fd = open(filename, flags, 0666);
    if (fd == -1) {
        fprintf(stderr, "mysh: cannot open '%s' for output: %s\n",
                filename, strerror(errno));
    }
    return fd;
}

// Set up file redirection for stdin (<) and stdout (> or >>)
static int apply_redirections(const Command *command)
{
    int fd;

    if (command->input_file != NULL) {
        fd = open_input(command->input_file);
        if (fd == -1) {
            return -1;
        }
        if (dup2(fd, STDIN_FILENO) == -1) {
            fprintf(stderr, "mysh: dup2(stdin): %s\n", strerror(errno));
            (void)close(fd);
            return -1;
        }
        if (close(fd) == -1) {
            fprintf(stderr, "mysh: close: %s\n", strerror(errno));
            return -1;
        }
    }

    if (command->output_file != NULL) {
        fd = open_output(command->output_file, command->append_output);
        if (fd == -1) {
            return -1;
        }
        if (dup2(fd, STDOUT_FILENO) == -1) {
            fprintf(stderr, "mysh: dup2(stdout): %s\n", strerror(errno));
            (void)close(fd);
            return -1;
        }
        if (close(fd) == -1) {
            fprintf(stderr, "mysh: close: %s\n", strerror(errno));
            return -1;
        }
    }

    return 0;
}

static void child_exec_external(const Command *command)
{
    if (apply_redirections(command) == -1) {
        _exit(EXIT_FAILURE);
    }

    execvp(command->argv[0], command->argv);
    fprintf(stderr, "mysh: %s: %s\n", command->argv[0], strerror(errno));
    _exit(EXIT_FAILURE);
}

// Child process execution: handle builtins or call execvp for external program
static void child_exec_command(const Command *command)
{
    if (is_builtin(command)) {
        int status;

        if (apply_redirections(command) == -1) {
            _exit(EXIT_FAILURE);
        }
        status = execute_builtin(command);
        _exit(status == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
    }

    child_exec_external(command);
}

static int wait_for_pid(pid_t pid)
{
    int status;

    while (1) {
        pid_t result = waitpid(pid, &status, 0);

        if (result == pid) {
            if (WIFSIGNALED(status)) {
                fprintf(stderr, "mysh: process %ld terminated by signal %d\n",
                        (long)pid, WTERMSIG(status));
                return 128 + WTERMSIG(status);
            }
            if (WIFEXITED(status)) {
                return WEXITSTATUS(status);
            }
            return 0;
        }

        if (result == -1 && errno == EINTR) {
            continue;
        }

        if (result == -1) {
            fprintf(stderr, "mysh: waitpid(%ld): %s\n",
                    (long)pid, strerror(errno));
            return -1;
        }
    }
}

// Execute a single command: run foreground builtins in-process, or fork a child
static int run_single_command(const Command *command, int background)
{
    pid_t pid;

    if (is_builtin(command) && !background) {
        int saved_stdin = -1;
        int saved_stdout = -1;
        int status;

        if (command->input_file != NULL) {
            saved_stdin = dup(STDIN_FILENO);
            if (saved_stdin == -1) {
                fprintf(stderr, "mysh: dup(stdin): %s\n", strerror(errno));
                return -1;
            }
        }
        if (command->output_file != NULL) {
            saved_stdout = dup(STDOUT_FILENO);
            if (saved_stdout == -1) {
                fprintf(stderr, "mysh: dup(stdout): %s\n", strerror(errno));
                if (saved_stdin != -1) {
                    (void)close(saved_stdin);
                }
                return -1;
            }
        }

        if (apply_redirections(command) == -1) {
            if (saved_stdout != -1) {
                (void)dup2(saved_stdout, STDOUT_FILENO);
                (void)close(saved_stdout);
            }
            if (saved_stdin != -1) {
                (void)dup2(saved_stdin, STDIN_FILENO);
                (void)close(saved_stdin);
            }
            return -1;
        }

        status = execute_builtin(command);

        if (saved_stdout != -1) {
            if (dup2(saved_stdout, STDOUT_FILENO) == -1) {
                fprintf(stderr, "mysh: restoring stdout: %s\n", strerror(errno));
                status = -1;
            }
            (void)close(saved_stdout);
        }
        if (saved_stdin != -1) {
            if (dup2(saved_stdin, STDIN_FILENO) == -1) {
                fprintf(stderr, "mysh: restoring stdin: %s\n", strerror(errno));
                status = -1;
            }
            (void)close(saved_stdin);
        }
        return status;
    }

    pid = fork();
    if (pid == -1) {
        fprintf(stderr, "mysh: fork: %s\n", strerror(errno));
        return -1;
    }

    if (pid == 0) {
        child_exec_command(command);
    }

    if (background) {
        // Track background job in process table without waiting
        if (process_add(pid, command->argv[0]) == -1) {
            fprintf(stderr, "mysh: process table is full\n");
            if (kill(pid, SIGTERM) == -1 && errno != ESRCH) {
                perror("kill");
            }
            (void)wait_for_pid(pid);
            return -1;
        }
        printf("[%ld] %s\n", (long)pid, command->argv[0]);
        fflush(stdout);
        return 0;
    }

    return wait_for_pid(pid);
}

// Execute a two-command pipeline by connecting stdout of cmd 1 to stdin of cmd 2
static int run_pipeline(const Job *job)
{
    int pipefd[2] = {-1, -1};
    pid_t pids[MAX_PIPE_COMMANDS];
    int i;
    int final_status = 0;

    if (pipe(pipefd) == -1) {
        fprintf(stderr, "mysh: pipe: %s\n", strerror(errno));
        return -1;
    }

    for (i = 0; i < job->command_count; ++i) {
        pid_t pid = fork();

        if (pid == -1) {
            int j;

            fprintf(stderr, "mysh: fork: %s\n", strerror(errno));
            if (close(pipefd[0]) == -1) {
                fprintf(stderr, "mysh: close(pipe read end): %s\n", strerror(errno));
            }
            if (close(pipefd[1]) == -1) {
                fprintf(stderr, "mysh: close(pipe write end): %s\n", strerror(errno));
            }

            for (j = 0; j < i; ++j) {
                if (kill(pids[j], SIGTERM) == -1 && errno != ESRCH) {
                    fprintf(stderr, "mysh: kill(%ld): %s\n",
                            (long)pids[j], strerror(errno));
                }
            }
            for (j = 0; j < i; ++j) {
                (void)wait_for_pid(pids[j]);
            }
            return -1;
        }

        if (pid == 0) {
            if (i == 0) {
                if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
                    fprintf(stderr, "mysh: dup2(pipe stdout): %s\n", strerror(errno));
                    _exit(EXIT_FAILURE);
                }
            } else {
                if (dup2(pipefd[0], STDIN_FILENO) == -1) {
                    fprintf(stderr, "mysh: dup2(pipe stdin): %s\n", strerror(errno));
                    _exit(EXIT_FAILURE);
                }
            }

            if (close(pipefd[0]) == -1) {
                fprintf(stderr, "mysh: close(pipe read end): %s\n", strerror(errno));
                _exit(EXIT_FAILURE);
            }
            if (close(pipefd[1]) == -1) {
                fprintf(stderr, "mysh: close(pipe write end): %s\n", strerror(errno));
                _exit(EXIT_FAILURE);
            }

            child_exec_command(&job->commands[i]);
        }

        pids[i] = pid;
    }

    if (close(pipefd[0]) == -1) {
        fprintf(stderr, "mysh: close(pipe read end): %s\n", strerror(errno));
    }
    if (close(pipefd[1]) == -1) {
        fprintf(stderr, "mysh: close(pipe write end): %s\n", strerror(errno));
    }

    if (job->background) {
        for (i = 0; i < job->command_count; ++i) {
            if (process_add(pids[i], job->commands[i].argv[0]) == -1) {
                fprintf(stderr, "mysh: process table is full\n");
                if (kill(pids[i], SIGTERM) == -1 && errno != ESRCH) {
                    perror("kill");
                }
                (void)wait_for_pid(pids[i]);
                continue;
            }
            printf("[%ld] %s\n", (long)pids[i], job->commands[i].argv[0]);
            fflush(stdout);
        }
        return 0;
    }

    for (i = 0; i < job->command_count; ++i) {
        int status = wait_for_pid(pids[i]);
        if (status != 0) {
            final_status = status;
        }
    }

    return final_status;
}

static int execute_job(const Job *job)
{
    if (job->command_count == 1) {
        return run_single_command(&job->commands[0], job->background);
    }
    return run_pipeline(job);
}

// Execute each parsed job on the input line sequentially
int execute_parsed_line(const ParsedLine *line)
{
    int i;
    int final_status = 0;

    if (line == NULL) {
        return -1;
    }

    for (i = 0; i < line->job_count; ++i) {
        int status = execute_job(&line->jobs[i]);
        if (status != 0) {
            final_status = status;
        }
    }

    return final_status;
}
