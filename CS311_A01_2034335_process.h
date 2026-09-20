#ifndef PROCESS_H
#define PROCESS_H

#include <sys/types.h>

#define MAX_PROCESSES 256
#define MAX_PROCESS_NAME 256

// Track active background processes spawned by the shell
typedef struct {
    pid_t pid;
    char name[MAX_PROCESS_NAME];
} Process;

// Process table management functions
void process_init(void);
int process_add(pid_t pid, const char *name);
int process_remove(pid_t pid);
int process_find(pid_t pid, Process *out);
// Display running background processes (for builtin 'ps')
void process_print(void);
// Non-blocking wait to reap any finished background child processes
void process_reap_finished(void);
// Clean up all background jobs before shell exit
void process_terminate_all(void);

#endif

