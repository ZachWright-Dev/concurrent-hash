#include "logger.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

long long logger_timestamp(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000000LL) + tv.tv_usec;
}

int logger_init(logger_t *logger, const char *path) {
    logger->file = fopen(path, "w");
    if (!logger->file) {
        return -1;
    }
    pthread_mutex_init(&logger->mutex, NULL);
    return 0;
}

void logger_close(logger_t *logger) {
    if (logger->file) {
        fclose(logger->file);
    }
    pthread_mutex_destroy(&logger->mutex);
}

static void logger_vwrite(logger_t *logger, const char *fmt, va_list args) {
    vfprintf(logger->file, fmt, args);
    fputc('\n', logger->file);
    fflush(logger->file);
}

void logger_thread_log(logger_t *logger, int priority, const char *fmt, ...) {
    if (!logger->file) {
        return;
    }

    pthread_mutex_lock(&logger->mutex);
    long long ts = logger_timestamp();
    fprintf(logger->file, "%lld: THREAD %d ", ts, priority);
    va_list args;
    va_start(args, fmt);
    vfprintf(logger->file, fmt, args);
    va_end(args);
    fputc('\n', logger->file);
    fflush(logger->file);
    pthread_mutex_unlock(&logger->mutex);
}

void logger_log(logger_t *logger, const char *fmt, ...) {
    if (!logger->file) {
        return;
    }

    pthread_mutex_lock(&logger->mutex);
    va_list args;
    va_start(args, fmt);
    logger_vwrite(logger, fmt, args);
    va_end(args);
    pthread_mutex_unlock(&logger->mutex);
}
