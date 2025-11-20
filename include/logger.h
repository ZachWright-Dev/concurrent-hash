#ifndef LOGGER_H
#define LOGGER_H

#include <pthread.h>
#include <stdio.h>

typedef struct {
    FILE *file;
    pthread_mutex_t mutex;
} logger_t;

int logger_init(logger_t *logger, const char *path);
void logger_close(logger_t *logger);
void logger_thread_log(logger_t *logger, int priority, const char *fmt, ...);
void logger_log(logger_t *logger, const char *fmt, ...);
long long logger_timestamp(void);

#endif
