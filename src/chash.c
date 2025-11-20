#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hash_table.h"
#include "logger.h"

#define COMMAND_FILE "commands.txt"
#define LOG_FILE "hash.log"
#define MAX_LINE_LEN 256

typedef enum {
    CMD_INSERT,
    CMD_DELETE,
    CMD_UPDATE,
    CMD_SEARCH,
    CMD_PRINT
} command_type_t;

typedef struct {
    command_type_t type;
    char name[MAX_NAME_LEN];
    uint32_t value;
    int priority;
} command_t;

typedef struct {
    hash_table_t table;
    pthread_rwlock_t table_lock;
    logger_t logger;
    size_t read_lock_acq;
    size_t read_lock_rel;
    size_t write_lock_acq;
    size_t write_lock_rel;
    pthread_mutex_t sched_mutex;
    pthread_cond_t sched_cond;
    int next_priority;
} app_context_t;

typedef struct {
    app_context_t *app;
    command_t *command;
} worker_arg_t;

static uint32_t jenkins_hash(const char *key);
static char *trim(char *str);
static bool parse_uint32(const char *token, uint32_t *value);
static bool parse_int(const char *token, int *value);
static int load_commands(const char *path, command_t **commands, size_t *count);
static int parse_command_line(char *line, command_t *command);
static void *worker_main(void *arg);
static void execute_command(app_context_t *app, const command_t *cmd, bool final_run);
static void perform_insert(app_context_t *app, const command_t *cmd);
static void perform_delete(app_context_t *app, const command_t *cmd);
static void perform_update(app_context_t *app, const command_t *cmd);
static void perform_search(app_context_t *app, const command_t *cmd);
static void perform_print(app_context_t *app, const command_t *cmd, bool final_run);
static void acquire_read_lock(app_context_t *app, int priority);
static void release_read_lock(app_context_t *app, int priority);
static void acquire_write_lock(app_context_t *app, int priority);
static void release_write_lock(app_context_t *app, int priority);
static void log_final_summary(app_context_t *app);

int main(void) {
    command_t *commands = NULL;
    size_t command_count = 0;

    if (load_commands(COMMAND_FILE, &commands, &command_count) != 0) {
        return EXIT_FAILURE;
    }

    app_context_t app;
    hash_table_init(&app.table);
    pthread_rwlock_init(&app.table_lock, NULL);
    pthread_mutex_init(&app.sched_mutex, NULL);
    pthread_cond_init(&app.sched_cond, NULL);
    app.read_lock_acq = app.read_lock_rel = 0;
    app.write_lock_acq = app.write_lock_rel = 0;
    app.next_priority = 0;

    if (logger_init(&app.logger, LOG_FILE) != 0) {
        fprintf(stderr, "Failed to open %s for writing.\n", LOG_FILE);
        free(commands);
        pthread_rwlock_destroy(&app.table_lock);
        pthread_mutex_destroy(&app.sched_mutex);
        pthread_cond_destroy(&app.sched_cond);
        hash_table_destroy(&app.table);
        return EXIT_FAILURE;
    }

    pthread_t *threads = (pthread_t *)calloc(command_count, sizeof(pthread_t));
    worker_arg_t *thread_args =
        (worker_arg_t *)calloc(command_count, sizeof(worker_arg_t));

    if (!threads || !thread_args) {
        fprintf(stderr, "Failed to allocate thread structures.\n");
        free(commands);
        free(threads);
        free(thread_args);
        logger_close(&app.logger);
        pthread_rwlock_destroy(&app.table_lock);
        pthread_mutex_destroy(&app.sched_mutex);
        pthread_cond_destroy(&app.sched_cond);
        hash_table_destroy(&app.table);
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < command_count; ++i) {
        thread_args[i].app = &app;
        thread_args[i].command = &commands[i];
        int rc = pthread_create(&threads[i], NULL, worker_main, &thread_args[i]);
        if (rc != 0) {
            fprintf(stderr, "Failed to create thread %zu (error %d).\n", i, rc);
            command_count = i;
            break;
        }
    }

    for (size_t i = 0; i < command_count; ++i) {
        pthread_join(threads[i], NULL);
    }

    command_t final_print = {
        .type = CMD_PRINT,
        .name = "",
        .value = 0,
        .priority = (int)command_count
    };
    execute_command(&app, &final_print, true);

    log_final_summary(&app);

    free(commands);
    free(threads);
    free(thread_args);
    logger_close(&app.logger);
    pthread_rwlock_destroy(&app.table_lock);
    pthread_mutex_destroy(&app.sched_mutex);
    pthread_cond_destroy(&app.sched_cond);
    hash_table_destroy(&app.table);

    return EXIT_SUCCESS;
}

static void *worker_main(void *arg) {
    worker_arg_t *data = (worker_arg_t *)arg;
    app_context_t *app = data->app;
    command_t *command = data->command;

    logger_thread_log(&app->logger, command->priority, "WAITING FOR MY TURN");
    pthread_mutex_lock(&app->sched_mutex);
    while (command->priority != app->next_priority) {
        pthread_cond_wait(&app->sched_cond, &app->sched_mutex);
    }
    logger_thread_log(&app->logger, command->priority, "AWAKENED FOR WORK");
    pthread_mutex_unlock(&app->sched_mutex);

    execute_command(app, command, false);

    pthread_mutex_lock(&app->sched_mutex);
    app->next_priority++;
    pthread_cond_broadcast(&app->sched_cond);
    pthread_mutex_unlock(&app->sched_mutex);

    return NULL;
}

static void execute_command(app_context_t *app, const command_t *cmd,
                            bool final_run) {
    (void)final_run;
    switch (cmd->type) {
        case CMD_INSERT:
            perform_insert(app, cmd);
            break;
        case CMD_DELETE:
            perform_delete(app, cmd);
            break;
        case CMD_UPDATE:
            perform_update(app, cmd);
            break;
        case CMD_SEARCH:
            perform_search(app, cmd);
            break;
        case CMD_PRINT:
            perform_print(app, cmd, final_run);
            break;
    }
}

static void perform_insert(app_context_t *app, const command_t *cmd) {
    uint32_t hash = jenkins_hash(cmd->name);
    logger_thread_log(&app->logger, cmd->priority, "INSERT,%u,%s,%u", hash,
                      cmd->name, cmd->value);
    acquire_write_lock(app, cmd->priority);
    table_status_t status =
        hash_table_insert(&app->table, hash, cmd->name, cmd->value);
    release_write_lock(app, cmd->priority);

    if (status == TABLE_OK) {
        printf("Inserted %u,%s,%u\n", hash, cmd->name, cmd->value);
    } else if (status == TABLE_DUPLICATE) {
        printf("Insert failed. Entry %u is a duplicate.\n", hash);
    } else {
        fprintf(stderr, "Insert failed for %s due to allocation error.\n",
                cmd->name);
    }
}

static void perform_delete(app_context_t *app, const command_t *cmd) {
    uint32_t hash = jenkins_hash(cmd->name);
    logger_thread_log(&app->logger, cmd->priority, "DELETE,%u,%s", hash,
                      cmd->name);
    acquire_write_lock(app, cmd->priority);
    record_snapshot_t removed;
    table_status_t status =
        hash_table_delete(&app->table, hash, &removed);
    release_write_lock(app, cmd->priority);

    if (status == TABLE_OK) {
        printf("Deleted record for %u,%s,%u\n", removed.hash, removed.name,
               removed.salary);
    } else {
        printf("Entry %u not deleted. Not in database.\n", hash);
    }
}

static void perform_update(app_context_t *app, const command_t *cmd) {
    uint32_t hash = jenkins_hash(cmd->name);
    logger_thread_log(&app->logger, cmd->priority, "UPDATE,%u,%s,%u", hash,
                      cmd->name, cmd->value);
    acquire_write_lock(app, cmd->priority);
    record_snapshot_t before;
    record_snapshot_t after;
    table_status_t status =
        hash_table_update(&app->table, hash, cmd->value, &before, &after);
    release_write_lock(app, cmd->priority);

    if (status == TABLE_OK) {
        printf("Updated record %u from %u,%s,%u to %u,%s,%u\n", hash,
               before.hash, before.name, before.salary, after.hash,
               after.name, after.salary);
    } else {
        printf("Update failed. Entry %u not found.\n", hash);
    }
}

static void perform_search(app_context_t *app, const command_t *cmd) {
    uint32_t hash = jenkins_hash(cmd->name);
    logger_thread_log(&app->logger, cmd->priority, "SEARCH,%u,%s", hash,
                      cmd->name);
    acquire_read_lock(app, cmd->priority);
    record_snapshot_t found;
    bool exists = hash_table_find(&app->table, hash, &found);
    release_read_lock(app, cmd->priority);

    if (exists) {
        printf("Found: %u,%s,%u\n", found.hash, found.name, found.salary);
    } else {
        printf("%s not found.\n", cmd->name);
    }
}

static void perform_print(app_context_t *app, const command_t *cmd,
                          bool final_run) {
    (void)final_run;
    logger_thread_log(&app->logger, cmd->priority, "PRINT");
    acquire_read_lock(app, cmd->priority);
    record_snapshot_t *records = NULL;
    size_t count = hash_table_snapshot(&app->table, &records);
    release_read_lock(app, cmd->priority);

    if (count == SIZE_MAX) {
        fprintf(stderr, "Unable to allocate memory for snapshot.\n");
        return;
    }

    printf("Current Database:\n");
    for (size_t i = 0; i < count; ++i) {
        printf("%u,%s,%u\n", records[i].hash, records[i].name,
               records[i].salary);
    }

    free(records);
}

static uint32_t jenkins_hash(const char *key) {
    uint32_t hash = 0;
    while (*key) {
        hash += (unsigned char)(*key);
        hash += (hash << 10);
        hash ^= (hash >> 6);
        key++;
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

static char *trim(char *str) {
    char *start = str;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    if (*start == '\0') {
        return start;
    }

    char *end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) {
        *end = '\0';
        --end;
    }

    return start;
}

static bool parse_uint32(const char *token, uint32_t *value) {
    errno = 0;
    char *end = NULL;
    unsigned long parsed = strtoul(token, &end, 10);
    if (errno != 0 || *end != '\0' || parsed > UINT32_MAX) {
        return false;
    }
    *value = (uint32_t)parsed;
    return true;
}

static bool parse_int(const char *token, int *value) {
    errno = 0;
    char *end = NULL;
    long parsed = strtol(token, &end, 10);
    if (errno != 0 || *end != '\0' || parsed < 0 || parsed > INT32_MAX) {
        return false;
    }
    *value = (int)parsed;
    return true;
}

static int load_commands(const char *path, command_t **commands,
                         size_t *count) {
    FILE *file = fopen(path, "r");
    if (!file) {
        fprintf(stderr, "Unable to open %s.\n", path);
        return -1;
    }

    char line[MAX_LINE_LEN];
    if (!fgets(line, sizeof(line), file)) {
        fprintf(stderr, "Command file is empty.\n");
        fclose(file);
        return -1;
    }

    char *first = trim(line);
    char *token = strtok(first, ",");
    if (!token || strcmp(token, "threads") != 0) {
        fprintf(stderr, "Command file must begin with threads entry.\n");
        fclose(file);
        return -1;
    }

    char *count_token = strtok(NULL, ",\r\n");
    if (!count_token) {
        fprintf(stderr, "Missing thread count.\n");
        fclose(file);
        return -1;
    }

    int total = 0;
    if (!parse_int(trim(count_token), &total) || total <= 0) {
        fprintf(stderr, "Invalid thread count value.\n");
        fclose(file);
        return -1;
    }

    command_t *parsed =
        (command_t *)calloc((size_t)total, sizeof(command_t));
    if (!parsed) {
        fprintf(stderr, "Unable to allocate command array.\n");
        fclose(file);
        return -1;
    }

    size_t idx = 0;
    while (idx < (size_t)total && fgets(line, sizeof(line), file)) {
        char *clean = trim(line);
        if (*clean == '\0') {
            continue;
        }
        if (parse_command_line(clean, &parsed[idx]) != 0) {
            fprintf(stderr, "Failed to parse command on line %zu.\n",
                    idx + 2);
            free(parsed);
            fclose(file);
            return -1;
        }
        idx++;
    }

    fclose(file);

    if (idx != (size_t)total) {
        fprintf(stderr, "Command count mismatch. Expected %d entries.\n", total);
        free(parsed);
        return -1;
    }

    *commands = parsed;
    *count = idx;
    return 0;
}

static int parse_command_line(char *line, command_t *command) {
    char buffer[MAX_LINE_LEN];
    strncpy(buffer, line, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char *tokens[4] = {0};
    int count = 0;
    char *token = strtok(buffer, ",\r\n");
    while (token && count < 4) {
        tokens[count++] = trim(token);
        token = strtok(NULL, ",\r\n");
    }

    if (count == 0) {
        return -1;
    }

    const char *cmd = tokens[0];

    if (strcmp(cmd, "insert") == 0) {
        if (count < 4) {
            return -1;
        }
        command->type = CMD_INSERT;
        strncpy(command->name, tokens[1], MAX_NAME_LEN - 1);
        command->name[MAX_NAME_LEN - 1] = '\0';
        if (!parse_uint32(tokens[2], &command->value)) {
            return -1;
        }
        if (!parse_int(tokens[3], &command->priority)) {
            return -1;
        }
        return 0;
    }

    if (strcmp(cmd, "delete") == 0) {
        if (count < 4) {
            return -1;
        }
        command->type = CMD_DELETE;
        strncpy(command->name, tokens[1], MAX_NAME_LEN - 1);
        command->name[MAX_NAME_LEN - 1] = '\0';
        if (!parse_int(tokens[3], &command->priority)) {
            return -1;
        }
        command->value = 0;
        return 0;
    }

    if (strcmp(cmd, "update") == 0) {
        if (count < 4) {
            return -1;
        }
        command->type = CMD_UPDATE;
        strncpy(command->name, tokens[1], MAX_NAME_LEN - 1);
        command->name[MAX_NAME_LEN - 1] = '\0';
        if (!parse_uint32(tokens[2], &command->value)) {
            return -1;
        }
        if (!parse_int(tokens[3], &command->priority)) {
            return -1;
        }
        return 0;
    }

    if (strcmp(cmd, "search") == 0) {
        if (count < 4) {
            return -1;
        }
        command->type = CMD_SEARCH;
        strncpy(command->name, tokens[1], MAX_NAME_LEN - 1);
        command->name[MAX_NAME_LEN - 1] = '\0';
        if (!parse_int(tokens[3], &command->priority)) {
            return -1;
        }
        command->value = 0;
        return 0;
    }

    if (strcmp(cmd, "print") == 0) {
        command->type = CMD_PRINT;
        command->name[0] = '\0';
        const char *priority_token = tokens[count - 1];
        if (!parse_int(priority_token, &command->priority)) {
            return -1;
        }
        command->value = 0;
        return 0;
    }

    return -1;
}

static void acquire_read_lock(app_context_t *app, int priority) {
    pthread_rwlock_rdlock(&app->table_lock);
    app->read_lock_acq++;
    logger_thread_log(&app->logger, priority, "READ LOCK ACQUIRED");
}

static void release_read_lock(app_context_t *app, int priority) {
    pthread_rwlock_unlock(&app->table_lock);
    app->read_lock_rel++;
    logger_thread_log(&app->logger, priority, "READ LOCK RELEASED");
}

static void acquire_write_lock(app_context_t *app, int priority) {
    pthread_rwlock_wrlock(&app->table_lock);
    app->write_lock_acq++;
    logger_thread_log(&app->logger, priority, "WRITE LOCK ACQUIRED");
}

static void release_write_lock(app_context_t *app, int priority) {
    pthread_rwlock_unlock(&app->table_lock);
    app->write_lock_rel++;
    logger_thread_log(&app->logger, priority, "WRITE LOCK RELEASED");
}

static void log_final_summary(app_context_t *app) {
    size_t total_acq = app->read_lock_acq + app->write_lock_acq;
    size_t total_rel = app->read_lock_rel + app->write_lock_rel;
    logger_log(&app->logger, "Number of lock acquisitions: %zu", total_acq);
    logger_log(&app->logger, "Number of lock releases: %zu", total_rel);

    record_snapshot_t *records = NULL;
    pthread_rwlock_rdlock(&app->table_lock);
    size_t count = hash_table_snapshot(&app->table, &records);
    pthread_rwlock_unlock(&app->table_lock);

    logger_log(&app->logger, "Final Table:");
    if (count == SIZE_MAX) {
        return;
    }
    if (count == 0 || !records) {
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        logger_log(&app->logger, "%u,%s,%u", records[i].hash, records[i].name,
                   records[i].salary);
    }
    free(records);
}
