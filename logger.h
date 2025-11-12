/*
 * logger.h
 *
 * Thread-safe logging utilities for the concurrent hash table project.
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <pthread.h>

typedef struct hash_logger {
    FILE *log_file;
    FILE *output_file;
    pthread_mutex_t mutex;
} hash_logger_t;

int logger_init(hash_logger_t *logger,
                const char *log_path,
                const char *output_path);

void logger_close(hash_logger_t *logger);

void logger_log_line(hash_logger_t *logger, int priority, const char *message);

void logger_logf(hash_logger_t *logger, int priority, const char *fmt, ...);

void logger_write_output(hash_logger_t *logger, const char *line, int append_newline);

#endif /* LOGGER_H */


