#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <pthread.h>

#include "commands.h"
#include "logger.h"

typedef struct scheduler {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int current_priority;
    size_t active_threads;
    command_list_t *command_list;
} scheduler_t;

int scheduler_init(scheduler_t *scheduler, command_list_t *command_list);
void scheduler_destroy(scheduler_t *scheduler);
void scheduler_wait_for_turn(scheduler_t *scheduler, command_t *command, hash_logger_t *logger);
void scheduler_finish_turn(scheduler_t *scheduler, command_t *command);

#endif /* SCHEDULER_H */


