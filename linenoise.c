/* linenoise.c -- guerrilla line editing library against the idea that a
 * line editing lib needs to be 20,000 lines of C code.
 *
 * You can find the latest source code at:
 *
 *   http://github.com/antirez/linenoise
 *
 * Does a number of crazy assumptions that happen to be true in 99.9999% of
 * the 2010 UNIX computers around.
 *
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2010-2023, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2010-2013, Pieter Noordhuis <pcnoordhuis at gmail dot com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  *  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ------------------------------------------------------------------------
 *
 * References:
 * - http://invisible-island.net/xterm/ctlseqs/ctlseqs.html
 * - http://www.3waylabs.com/nw/WWW/products/wizcon/vt220.html
 *
 * Todo list:
 * - Filter bogus Ctrl+<char> combinations.
 * - Win32 support
 *
 * Bloat:
 * - History search like Ctrl+r in readline?
 *
 * List of escape sequences used by this program, we do everything just
 * with three sequences. In order to be so cheap we may have some
 * flickering effect with some slow terminal, but the lesser sequences
 * the more compatible.
 *
 * EL (Erase Line)
 *    Sequence: ESC [ n K
 *    Effect: if n is 0 or missing, clear from cursor to end of line
 *    Effect: if n is 1, clear from beginning of line to cursor
 *    Effect: if n is 2, clear entire line
 *
 * CUF (CUrsor Forward)
 *    Sequence: ESC [ n C
 *    Effect: moves cursor forward n chars
 *
 * CUB (CUrsor Backward)
 *    Sequence: ESC [ n D
 *    Effect: moves cursor backward n chars
 *
 * The following is used to get the terminal width if getting
 * the width with the TIOCGWINSZ ioctl fails
 *
 * DSR (Device Status Report)
 *    Sequence: ESC [ 6 n
 *    Effect: reports the current cusor position as ESC [ n ; m R
 *            where n is the row and m is the column
 *
 * When multi line mode is enabled, we also use an additional escape
 * sequence. However multi line editing is disabled by default.
 *
 * CUU (Cursor Up)
 *    Sequence: ESC [ n A
 *    Effect: moves cursor up of n chars.
 *
 * CUD (Cursor Down)
 *    Sequence: ESC [ n B
 *    Effect: moves cursor down of n chars.
 *
 * When linenoiseClearScreen() is called, two additional escape sequences
 * are used in order to clear the screen and position the cursor at home
 * position.
 *
 * CUP (Cursor position)
 *    Sequence: ESC [ H
 *    Effect: moves the cursor to upper left corner
 *
 * ED (Erase display)
 *    Sequence: ESC [ 2 J
 *    Effect: clear the whole screen
 *
 */

#define _POSIX_C_SOURCE 200809L
#include "linenoise.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define INITIAL_BUFFER_SIZE 128
#define HISTORY_LIMIT 100

static struct termios orig_termios;
static int raw_mode_enabled = 0;
static char **history = NULL;
static size_t history_len = 0;
static size_t history_cap = 0;
static size_t history_max_len = HISTORY_LIMIT;

static void disable_raw_mode(void)
{
    if (raw_mode_enabled) {
        (void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        raw_mode_enabled = 0;
    }
}

// Switch terminal to raw mode (disable canonical mode and echo)
static int enable_raw_mode(void)
{
    struct termios raw;

    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        return -1;
    }

    raw = orig_termios;
    raw.c_lflag &= (tcflag_t) ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_iflag &= (tcflag_t) ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= (tcflag_t) ~(OPOST);
    raw.c_cflag |= CS8;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        return -1;
    }

    raw_mode_enabled = 1;
    return 0;
}

static void clear_current_line(const char *prompt, size_t old_len)
{
    size_t i;

    fputs("\r", stdout);
    fputs(prompt, stdout);
    for (i = 0; i < old_len; ++i) {
        fputc(' ', stdout);
    }
    fputs("\r", stdout);
    fputs(prompt, stdout);
    fflush(stdout);
}

static int set_line_buffer(char **buffer, size_t *length, size_t *capacity,
                           const char *value)
{
    size_t new_len = strlen(value);

    if (new_len + 1 > *capacity) {
        char *new_buffer;
        size_t new_capacity = *capacity;

        while (new_len + 1 > new_capacity) {
            new_capacity *= 2;
        }

        new_buffer = realloc(*buffer, new_capacity);
        if (new_buffer == NULL) {
            return -1;
        }
        *buffer = new_buffer;
        *capacity = new_capacity;
    }

    memcpy(*buffer, value, new_len + 1);
    *length = new_len;
    return 0;
}

int linenoiseHistorySetMaxLen(int len)
{
    size_t target;

    if (len <= 0) {
        return -1;
    }

    target = (size_t)len;
    history_max_len = target;

    while (history_len > history_max_len) {
        free(history[0]);
        memmove(history, history + 1, (history_len - 1) * sizeof(history[0]));
        --history_len;
    }

    return 1;
}

// Append command line to history, ignoring consecutive duplicates
int linenoiseHistoryAdd(const char *line)
{
    char *copy;

    if (line == NULL || *line == '\0' || history_max_len == 0) {
        return 0;
    }

    if (history_len > 0 && strcmp(history[history_len - 1], line) == 0) {
        return 1;
    }

    copy = strdup(line);
    if (copy == NULL) {
        return -1;
    }

    if (history_len == history_cap) {
        size_t new_cap = history_cap == 0 ? 16 : history_cap * 2;
        char **new_history = realloc(history, new_cap * sizeof(*new_history));
        if (new_history == NULL) {
            free(copy);
            return -1;
        }
        history = new_history;
        history_cap = new_cap;
    }

    if (history_len == history_max_len) {
        free(history[0]);
        memmove(history, history + 1,
                (history_len - 1) * sizeof(history[0]));
        --history_len;
    }

    history[history_len++] = copy;
    return 1;
}

void linenoiseFree(void *ptr)
{
    free(ptr);
}

static void free_history(void)
{
    size_t i;

    for (i = 0; i < history_len; ++i) {
        free(history[i]);
    }
    free(history);
    history = NULL;
    history_len = 0;
    history_cap = 0;
}

// Restore original terminal settings and free history memory at exit
static void restore_terminal_at_exit(void)
{
    disable_raw_mode();
    free_history();
}

static int read_byte(unsigned char *c)
{
    ssize_t n;

    do {
        n = read(STDIN_FILENO, c, 1);
    } while (n == -1 && errno == EINTR);

    if (n == 0) {
        return 0;
    }
    if (n == -1) {
        return -1;
    }
    return 1;
}

// Read interactive input line, handling editing keypresses and history
char *linenoise(const char *prompt)
{
    char *buffer = NULL;
    size_t capacity = INITIAL_BUFFER_SIZE;
    size_t length = 0;
    size_t cursor = 0;
    size_t history_index = history_len;
    int interactive;

    if (prompt == NULL) {
        prompt = "";
    }

    interactive = isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
    buffer = malloc(capacity);
    if (buffer == NULL) {
        return NULL;
    }
    buffer[0] = '\0';

    fputs(prompt, stdout);
    fflush(stdout);

    if (!interactive) {
        size_t used = 0;
        int c;

        while ((c = getchar()) != EOF && c != '\n') {
            if (used + 1 >= capacity) {
                char *new_buffer;
                capacity *= 2;
                new_buffer = realloc(buffer, capacity);
                if (new_buffer == NULL) {
                    free(buffer);
                    return NULL;
                }
                buffer = new_buffer;
            }
            buffer[used++] = (char)c;
        }
        buffer[used] = '\0';
        if (c == EOF && used == 0) {
            free(buffer);
            return NULL;
        }
        return buffer;
    }

    if (enable_raw_mode() == -1) {
        free(buffer);
        fputc('\n', stdout);
        return NULL;
    }
    {
        static int atexit_registered = 0;
        if (!atexit_registered) {
            atexit(restore_terminal_at_exit);
            atexit_registered = 1;
        }
    }

    while (1) {
        unsigned char c;
        int rc = read_byte(&c);

        if (rc == 0) {
            disable_raw_mode();
            fputc('\n', stdout);
            free(buffer);
            return NULL;
        }
        if (rc == -1) {
            disable_raw_mode();
            fputc('\n', stdout);
            free(buffer);
            return NULL;
        }

        if (c == 4) { /* Ctrl-D */
            if (length == 0) {
                disable_raw_mode();
                fputc('\n', stdout);
                free(buffer);
                return NULL;
            }
            continue;
        }

        if (c == 3) { /* Ctrl-C */
            disable_raw_mode();
            fputs("^C\n", stdout);
            fflush(stdout);
            free(buffer);
            return strdup("");
        }

        if (c == '\r' || c == '\n') {
            fputc('\n', stdout);
            fflush(stdout);
            disable_raw_mode();
            buffer[length] = '\0';
            return buffer;
        }

        if (c == 127 || c == 8) { /* Backspace */
            if (cursor > 0) {
                memmove(buffer + cursor - 1, buffer + cursor,
                        length - cursor + 1);
                --cursor;
                --length;
                fputs("\b", stdout);
                fputs(buffer + cursor, stdout);
                fputc(' ', stdout);
                for (size_t i = cursor; i < length + 1; ++i) {
                    fputs("\b", stdout);
                }
                fflush(stdout);
            }
            continue;
        }

        if (c == 27) { /* Escape sequence */
            unsigned char seq1, seq2;
            if (read_byte(&seq1) <= 0 || seq1 != '[' || read_byte(&seq2) <= 0) {
                continue;
            }

            if (seq2 == 'A') { /* Up */
                if (history_index > 0) {
                    size_t old_len = length;
                    --history_index;
                    if (set_line_buffer(&buffer, &length, &capacity,
                                        history[history_index]) == -1) {
                        disable_raw_mode();
                        free(buffer);
                        return NULL;
                    }
                    cursor = length;
                    clear_current_line(prompt, old_len);
                    fputs(buffer, stdout);
                    fflush(stdout);
                }
            } else if (seq2 == 'B') { /* Down */
                size_t old_len = length;

                if (history_index < history_len) {
                    ++history_index;
                    if (history_index == history_len) {
                        if (set_line_buffer(&buffer, &length, &capacity, "") == -1) {
                            disable_raw_mode();
                            free(buffer);
                            return NULL;
                        }
                    } else if (set_line_buffer(&buffer, &length, &capacity,
                                               history[history_index]) == -1) {
                        disable_raw_mode();
                        free(buffer);
                        return NULL;
                    }
                    cursor = length;
                    clear_current_line(prompt, old_len);
                    fputs(buffer, stdout);
                    fflush(stdout);
                }
            } else if (seq2 == 'D') { /* Left */
                if (cursor > 0) {
                    --cursor;
                    fputs("\033[D", stdout);
                    fflush(stdout);
                }
            } else if (seq2 == 'C') { /* Right */
                if (cursor < length) {
                    ++cursor;
                    fputs("\033[C", stdout);
                    fflush(stdout);
                }
            }
            continue;
        }

        if (c >= 32 && c <= 126) {
            if (length + 1 >= capacity) {
                char *new_buffer;
                capacity *= 2;
                new_buffer = realloc(buffer, capacity);
                if (new_buffer == NULL) {
                    disable_raw_mode();
                    free(buffer);
                    return NULL;
                }
                buffer = new_buffer;
            }

            memmove(buffer + cursor + 1, buffer + cursor,
                    length - cursor + 1);
            buffer[cursor] = (char)c;
            ++cursor;
            ++length;

            if (cursor == length) {
                fputc(c, stdout);
            } else {
                fputs(buffer + cursor - 1, stdout);
                for (size_t i = cursor; i < length; ++i) {
                    fputs("\b", stdout);
                }
            }
            fflush(stdout);
            history_index = history_len;
        }
    }
}
