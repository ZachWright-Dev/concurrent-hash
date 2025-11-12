#ifndef COMMANDS_H
#define COMMANDS_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    COMMAND_INSERT,
    COMMAND_UPDATE,
    COMMAND_DELETE,
    COMMAND_SEARCH,
    COMMAND_PRINT,
    COMMAND_UNKNOWN
} command_type_t;

typedef enum {
    CMD_STATE_PENDING,
    CMD_STATE_WAITING,
    CMD_STATE_ACTIVE,
    CMD_STATE_FINISHED
} command_state_t;

typedef struct command {
    command_type_t type;
    command_state_t state;
    char name[128];
    uint32_t salary;
    int priority;
    char raw_line[256];
} command_t;

typedef struct command_list {
    command_t *commands;
    size_t count;
    size_t capacity;
} command_list_t;

int command_list_init(command_list_t *list);
void command_list_free(command_list_t *list);
int command_list_append(command_list_t *list, const command_t *command);
int parse_commands_file(const char *path, command_list_t *list);

command_type_t command_type_from_string(const char *str);
const char *command_type_to_string(command_type_t type);

#endif /* COMMANDS_H */


