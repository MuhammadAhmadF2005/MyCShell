#ifndef LINENOISE_H
#define LINENOISE_H

#ifdef __cplusplus
extern "C" {
#endif

char *linenoise(const char *prompt);
void linenoiseFree(void *ptr);
int linenoiseHistoryAdd(const char *line);
int linenoiseHistorySetMaxLen(int len);

#ifdef __cplusplus
}
#endif

#endif
