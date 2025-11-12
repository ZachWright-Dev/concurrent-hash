#include "logger.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

static long long current_timestamp_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000LL) + (tv.tv_usec / 1000LL);
}

int logger_init(hash_logger_t *logger,
                const char *log_path,
                const char *output_path) {
    if (!logger || !log_path || !output_path) {
        return -1;
    }

    memset(logger, 0, sizeof(*logger));

    logger->log_file = fopen(log_path, "w");
    if (!logger->log_file) {
        return -1;
    }

    logger->output_file = fopen(output_path, "w");
    if (!logger->output_file) {
        fclose(logger->log_file);
        logger->log_file = NULL;
        return -1;
    }

    if (pthread_mutex_init(&logger->mutex, NULL) != 0) {
        fclose(logger->log_file);
        fclose(logger->output_file);
        logger->log_file = NULL;
        logger->output_file = NULL;
        return -1;
    }

    return 0;
}

void logger_close(hash_logger_t *logger) {
    if (!logger) {
        return;
    }

    pthread_mutex_destroy(&logger->mutex);

    if (logger->log_file) {
        fclose(logger->log_file);
        logger->log_file = NULL;
    }

    if (logger->output_file) {
        fclose(logger->output_file);
        logger->output_file = NULL;
    }
}

void logger_log_line(hash_logger_t *logger, int priority, const char *message) {
    if (!logger || !message) {
        return;
    }

    long long ts = current_timestamp_ms();

    pthread_mutex_lock(&logger->mutex);
    if (logger->log_file) {
        fprintf(logger->log_file, "%lld,THREAD %d:%s\n", ts, priority, message);
        fflush(logger->log_file);
    }
    pthread_mutex_unlock(&logger->mutex);
}

void logger_logf(hash_logger_t *logger, int priority, const char *fmt, ...) {
    if (!logger || !fmt) {
        return;
    }

    char buffer[512];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    logger_log_line(logger, priority, buffer);
}

void logger_write_output(hash_logger_t *logger, const char *line, int append_newline) {
    if (!logger || !line) {
        return;
    }

    pthread_mutex_lock(&logger->mutex);
    if (logger->output_file) {
        fputs(line, logger->output_file);
        if (append_newline) {
            fputc('\n', logger->output_file);
        }
        fflush(logger->output_file);
    }
    pthread_mutex_unlock(&logger->mutex);
}


