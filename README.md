# CS311 Assignment 01 — mysh: A Mini Unix Shell

**Course:** CS311 — Operating Systems  
**Institution:** Faculty of Computer Science and Engineering, GIKI  
**Semester:** Fall 2026  
**Student ID:** 2034335  

---

## Overview

This repository contains the implementation of **mysh** (MY SHell), a minimal Unix shell written in C, developed as part of the CS311 Operating Systems course. The shell demonstrates core operating system concepts including process creation, I/O redirection, inter-process communication via pipes, and signal handling, using POSIX system calls throughout.

---

## Features

- Interactive prompt displaying the current working directory
- Command execution via `fork()` and `execvp()`
- Foreground and background process execution (`&` operator)
- Shell builtins: `cd`, `pwd`, `ps`, `kill`, `exit`
- Internal process table for tracking background jobs
- I/O redirection: `>` (overwrite), `>>` (append), `<` (input)
- Two-command pipelines using `|`
- Single and double quote handling in command arguments
- Command history via the linenoise library
- Comprehensive error reporting for all system call failures

---

## File Structure

```
CS311_A01_2034335_main.c       Entry point and REPL loop
CS311_A01_2034335_shell.c      Command and pipeline execution
CS311_A01_2034335_shell.h
CS311_A01_2034335_parser.c     Input tokenizer and command parser
CS311_A01_2034335_parser.h
CS311_A01_2034335_builtins.c   Built-in command implementations
CS311_A01_2034335_builtins.h
CS311_A01_2034335_process.c    Background process table management
CS311_A01_2034335_process.h
CS311_A01_2034335_linenoise.c  Self-contained linenoise line editor
CS311_A01_2034335_linenoise.h
Makefile
```

---

## Build Instructions

The project requires GCC and GNU Make running on a Linux environment (or WSL on Windows).

**Dependencies:**

- GCC 10 or later
- GNU Make
- POSIX-compatible operating system (Linux or WSL)

**To build:**

```sh
make
```

This compiles all sources with `-Wall -Wextra -std=c11 -g` and produces the `mysh` executable.

**To clean build artefacts:**

```sh
make clean
```

---

## Running the Shell

**Interactive mode:**

```sh
./mysh
```

The shell displays a welcome banner and an interactive prompt of the form `mysh:<cwd>$`.

**Non-interactive mode (script input):**

```sh
./mysh < script.sh
```

In non-interactive mode, the welcome banner is suppressed and the shell reads commands line by line from the provided file.

---

## Usage Examples

```sh
# Run a command with arguments
ls -la

# Change directory
cd /tmp
cd ~
cd -

# Output redirection
echo "hello" > output.txt
echo "world" >> output.txt

# Input redirection
cat < output.txt

# Pipeline
cat output.txt | wc -l

# Background execution
sleep 10 &

# List background jobs
ps

# Kill a background job
kill <PID>

# Exit the shell
exit 0
```

---

## System Calls Used

| System Call | Purpose |
|:---|:---|
| `fork()` | Create child processes for command execution |
| `execvp()` | Replace child process image with target command |
| `waitpid()` | Wait for child processes to terminate |
| `pipe()` | Create anonymous pipe for inter-process communication |
| `dup2()` | Redirect file descriptors for I/O redirection |
| `open()` / `close()` | Open and close files for redirection |
| `kill()` | Send SIGTERM to background processes |
| `chdir()` | Change working directory for `cd` builtin |
| `getcwd()` | Retrieve current working directory for prompt |
| `getenv()` / `setenv()` | Read and update environment variables |
| `isatty()` | Detect interactive vs. non-interactive mode |

---

## Error Handling

All system call return values are checked. Errors are reported to `stderr` with descriptive messages using `strerror(errno)` or `perror()`. The shell does not crash on invalid user input; instead, it prints a helpful error message and continues execution.

---

## References

1. W. Richard Stevens and Stephen A. Rago, *Advanced Programming in the UNIX Environment*, 3rd ed., Addison-Wesley, 2013.
2. Abraham Silberschatz, Peter B. Galvin, and Greg Gagne, *Operating System Concepts*, 10th ed., Wiley, 2018.
3. The Open Group, *The Single UNIX Specification (POSIX.1-2017)*. Available: https://pubs.opengroup.org/onlinepubs/9699919799/
4. Salvatore Sanfilippo (antirez), *linenoise — A minimal, zero-config, BSD licensed, readline replacement*. Available: https://github.com/antirez/linenoise
5. Linux `man` pages: `fork(2)`, `execvp(3)`, `waitpid(2)`, `pipe(2)`, `dup2(2)`, `kill(2)`, `chdir(2)`.

---

## Acknowledgements

The author thanks the CS311 (Operating Systems) course instructor Dr. Taj Muhammad Khan, for the assignment specification and for providing guidance on POSIX system programming. The linenoise library by Salvatore Sanfilippo was used for interactive line editing and command history, as required by the assignment instructions.
