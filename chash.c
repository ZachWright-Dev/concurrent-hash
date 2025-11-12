#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "commands.h"
#include "hash_table.h"
#include "logger.h"
#include "scheduler.h"

#define COMMANDS_FILE "commands.txt"
#define LOG_FILE "hash.log"
#define OUTPUT_FILE "output.txt"

typedef struct worker_args {
    hash_table_t *table;
    pthread_rwlock_t *table_lock;
    scheduler_t *scheduler;
    hash_logger_t *logger;
    command_t *command;
} worker_args_t;

static void log_command_start(hash_logger_t *logger, const command_t *command) {
    if (!logger || !command) {
        return;
    }

    switch (command->type) {
        case COMMAND_INSERT:
            logger_logf(logger, command->priority, "INSERT,%s,%u,%d",
                        command->name, command->salary, command->priority);
            break;
        case COMMAND_UPDATE:
            logger_logf(logger, command->priority, "UPDATE,%s,%u,%d",
                        command->name, command->salary, command->priority);
            break;
        case COMMAND_DELETE:
            logger_logf(logger, command->priority, "DELETE,%s,%d",
                        command->name, command->priority);
            break;
        case COMMAND_SEARCH:
            logger_logf(logger, command->priority, "SEARCH,%s,%d",
                        command->name, command->priority);
            break;
        case COMMAND_PRINT:
            logger_logf(logger, command->priority, "PRINT,%d", command->priority);
            break;
        default:
            logger_logf(logger, command->priority, "UNKNOWN COMMAND");
            break;
    }
}

static void perform_insert(hash_table_t *table, const command_t *command) {
    hash_record_t record = {0};
    uint32_t previous = 0;
    hash_insert_status_t status = hash_table_insert(table, command->name, command->salary,
                                                    &record, &previous);

    if (status == HASH_INSERTED) {
        printf("INSERT Inserted %s,%u (hash=%u)\n", record.name, record.salary, record.hash);
    } else if (status == HASH_UPDATED) {
        printf("INSERT Updated %s from %u to %u (hash=%u)\n",
               record.name, previous, record.salary, record.hash);
    } else {
        printf("INSERT Failed for %s\n", command->name);
    }
}

static void perform_update(hash_table_t *table, const command_t *command) {
    hash_record_t record = {0};
    uint32_t previous = 0;
    int updated = hash_table_update(table, command->name, command->salary,
                                    &record, &previous);

    if (updated) {
        printf("UPDATE Updated %s from %u to %u (hash=%u)\n",
               record.name, previous, record.salary, record.hash);
    } else {
        printf("UPDATE No record found for %s\n", command->name);
    }
}

static void perform_delete(hash_table_t *table, const command_t *command) {
    hash_record_t record = {0};
    int deleted = hash_table_delete(table, command->name, &record);

    if (deleted) {
        printf("DELETE Deleted record for %s,%u (hash=%u)\n",
               record.name, record.salary, record.hash);
    } else {
        printf("DELETE No record found for %s\n", command->name);
    }
}

static void perform_search(hash_table_t *table,
                           const command_t *command,
                           hash_logger_t *logger) {
    hash_record_t record = {0};
    int found = hash_table_search(table, command->name, &record);
    char buffer[256];

    if (found) {
        printf("SEARCH Found %s,%u (hash=%u)\n", record.name, record.salary, record.hash);
        snprintf(buffer, sizeof(buffer), "%s,%u", record.name, record.salary);
        logger_write_output(logger, buffer, 1);
    } else {
        printf("SEARCH No record found for %s\n", command->name);
        snprintf(buffer, sizeof(buffer), "No record found for %s", command->name);
        logger_write_output(logger, buffer, 1);
    }
}

static void perform_print(hash_table_t *table, hash_logger_t *logger) {
    size_t count = 0;
    hash_record_t *records = hash_table_snapshot(table, &count);

    printf("PRINT Current Database:\n");
    logger_write_output(logger, "PRINT Current Database:", 1);

    if (!records || count == 0) {
        printf("    <empty>\n");
        logger_write_output(logger, "    <empty>", 1);
    } else {
        for (size_t i = 0; i < count; ++i) {
            char buffer[256];
            snprintf(buffer, sizeof(buffer), "    %u: %s,%u",
                     records[i].hash, records[i].name, records[i].salary);
            printf("%s\n", buffer);
            logger_write_output(logger, buffer, 1);
        }
    }

    logger_write_output(logger, "", 1);
    hash_table_free_snapshot(records, count);
}

static void *command_worker(void *arg) {
    worker_args_t *ctx = (worker_args_t *)arg;
    command_t *command = ctx->command;

    log_command_start(ctx->logger, command);
    scheduler_wait_for_turn(ctx->scheduler, command, ctx->logger);

    int is_read_operation = (command->type == COMMAND_SEARCH || command->type == COMMAND_PRINT);

    if (is_read_operation) {
        pthread_rwlock_rdlock(ctx->table_lock);
        logger_log_line(ctx->logger, command->priority, "READ LOCK ACQUIRED");
    } else {
        pthread_rwlock_wrlock(ctx->table_lock);
        logger_log_line(ctx->logger, command->priority, "WRITE LOCK ACQUIRED");
    }

    switch (command->type) {
        case COMMAND_INSERT:
            perform_insert(ctx->table, command);
            break;
        case COMMAND_UPDATE:
            perform_update(ctx->table, command);
            break;
        case COMMAND_DELETE:
            perform_delete(ctx->table, command);
            break;
        case COMMAND_SEARCH:
            perform_search(ctx->table, command, ctx->logger);
            break;
        case COMMAND_PRINT:
            perform_print(ctx->table, ctx->logger);
            break;
        default:
            printf("Unknown command encountered.\n");
            break;
    }

    pthread_rwlock_unlock(ctx->table_lock);
    if (is_read_operation) {
        logger_log_line(ctx->logger, command->priority, "READ LOCK RELEASED");
    } else {
        logger_log_line(ctx->logger, command->priority, "WRITE LOCK RELEASED");
    }

    scheduler_finish_turn(ctx->scheduler, command);
    return NULL;
}

int main(void) {
    command_list_t command_list;
    if (parse_commands_file(COMMANDS_FILE, &command_list) != 0) {
        fprintf(stderr, "Failed to read or parse commands from %s\n", COMMANDS_FILE);
        return EXIT_FAILURE;
    }

    if (command_list.count == 0) {
        fprintf(stderr, "No commands to process.\n");
        command_list_free(&command_list);
        return EXIT_FAILURE;
    }

    hash_table_t table;
    hash_table_init(&table);

    hash_logger_t logger;
    if (logger_init(&logger, LOG_FILE, OUTPUT_FILE) != 0) {
        fprintf(stderr, "Failed to initialise log files.\n");
        command_list_free(&command_list);
        return EXIT_FAILURE;
    }

    pthread_rwlock_t table_lock;
    if (pthread_rwlock_init(&table_lock, NULL) != 0) {
        fprintf(stderr, "Failed to initialise reader-writer lock.\n");
        logger_close(&logger);
        command_list_free(&command_list);
        return EXIT_FAILURE;
    }

    scheduler_t scheduler;
    if (scheduler_init(&scheduler, &command_list) != 0) {
        fprintf(stderr, "Failed to initialise scheduler.\n");
        pthread_rwlock_destroy(&table_lock);
        logger_close(&logger);
        command_list_free(&command_list);
        return EXIT_FAILURE;
    }

    pthread_t *threads = calloc(command_list.count, sizeof(pthread_t));
    worker_args_t *contexts = calloc(command_list.count, sizeof(worker_args_t));

    if (!threads || !contexts) {
        fprintf(stderr, "Failed to allocate thread resources.\n");
        free(threads);
        free(contexts);
        scheduler_destroy(&scheduler);
        pthread_rwlock_destroy(&table_lock);
        logger_close(&logger);
        command_list_free(&command_list);
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < command_list.count; ++i) {
        contexts[i].table = &table;
        contexts[i].table_lock = &table_lock;
        contexts[i].scheduler = &scheduler;
        contexts[i].logger = &logger;
        contexts[i].command = &command_list.commands[i];

        if (pthread_create(&threads[i], NULL, command_worker, &contexts[i]) != 0) {
            fprintf(stderr, "Failed to create thread for command %zu\n", i);
        }
    }

    for (size_t i = 0; i < command_list.count; ++i) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    free(contexts);

    scheduler_destroy(&scheduler);
    pthread_rwlock_destroy(&table_lock);
    logger_close(&logger);
    command_list_free(&command_list);

    return EXIT_SUCCESS;
}


