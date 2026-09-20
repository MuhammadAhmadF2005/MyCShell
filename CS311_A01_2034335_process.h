#ifndef PROCESS_H
#define PROCESS_H

#include <sys/types.h>

#define MAX_PROCESSES 256
#define MAX_PROCESS_NAME 256

typedef struct {
    pid_t pid;
    char name[MAX_PROCESS_NAME];
} Process;

void process_init(void);
int process_add(pid_t pid, const char *name);
int process_remove(pid_t pid);
int process_find(pid_t pid, Process *out);
void process_print(void);
void process_reap_finished(void);
void process_terminate_all(void);

#endif
