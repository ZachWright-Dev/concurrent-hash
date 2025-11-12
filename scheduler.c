#include "scheduler.h"

#include <limits.h>

static int scheduler_waiting_min_priority_locked(scheduler_t *scheduler) {
    int min_priority = INT_MAX;

    if (!scheduler || !scheduler->command_list) {
        return min_priority;
    }

    for (size_t i = 0; i < scheduler->command_list->count; ++i) {
        command_t *cmd = &scheduler->command_list->commands[i];
        if (cmd->state == CMD_STATE_WAITING) {
            if (cmd->priority < min_priority) {
                min_priority = cmd->priority;
            }
        }
    }

    return min_priority;
}

int scheduler_init(scheduler_t *scheduler, command_list_t *command_list) {
    if (!scheduler || !command_list) {
        return -1;
    }

    scheduler->command_list = command_list;
    scheduler->current_priority = INT_MAX;
    scheduler->active_threads = 0;

    if (pthread_mutex_init(&scheduler->mutex, NULL) != 0) {
        return -1;
    }

    if (pthread_cond_init(&scheduler->cond, NULL) != 0) {
        pthread_mutex_destroy(&scheduler->mutex);
        return -1;
    }

    return 0;
}

void scheduler_destroy(scheduler_t *scheduler) {
    if (!scheduler) {
        return;
    }

    pthread_mutex_destroy(&scheduler->mutex);
    pthread_cond_destroy(&scheduler->cond);
}

void scheduler_wait_for_turn(scheduler_t *scheduler, command_t *command, hash_logger_t *logger) {
    if (!scheduler || !command) {
        return;
    }

    pthread_mutex_lock(&scheduler->mutex);

    command->state = CMD_STATE_WAITING;
    logger_log_line(logger, command->priority, "WAITING FOR MY TURN");

    if (scheduler->active_threads == 0) {
        int min_pri = scheduler_waiting_min_priority_locked(scheduler);
        if (min_pri == INT_MAX || command->priority < min_pri) {
            scheduler->current_priority = command->priority;
        } else {
            scheduler->current_priority = min_pri;
        }
        pthread_cond_broadcast(&scheduler->cond);
    } else if (scheduler->current_priority == INT_MAX) {
        scheduler->current_priority = command->priority;
        pthread_cond_broadcast(&scheduler->cond);
    }

    while (command->priority != scheduler->current_priority) {
        pthread_cond_wait(&scheduler->cond, &scheduler->mutex);
        if (scheduler->active_threads == 0) {
            int min_pri = scheduler_waiting_min_priority_locked(scheduler);
            if (min_pri != INT_MAX) {
                scheduler->current_priority = min_pri;
                pthread_cond_broadcast(&scheduler->cond);
            }
        }
    }

    command->state = CMD_STATE_ACTIVE;
    scheduler->active_threads++;
    logger_log_line(logger, command->priority, "AWAKENED FOR WORK");

    pthread_mutex_unlock(&scheduler->mutex);
}

void scheduler_finish_turn(scheduler_t *scheduler, command_t *command) {
    if (!scheduler || !command) {
        return;
    }

    pthread_mutex_lock(&scheduler->mutex);

    command->state = CMD_STATE_FINISHED;
    if (scheduler->active_threads > 0) {
        scheduler->active_threads--;
    }

    if (scheduler->active_threads == 0) {
        int min_pri = scheduler_waiting_min_priority_locked(scheduler);
        if (min_pri == INT_MAX) {
            scheduler->current_priority = INT_MAX;
        } else {
            scheduler->current_priority = min_pri;
        }
        pthread_cond_broadcast(&scheduler->cond);
    }

    pthread_mutex_unlock(&scheduler->mutex);
}


