#include "commands.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static void trim_whitespace(char *str) {
    if (!str) {
        return;
    }

    char *start = str;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)*(end - 1))) {
        end--;
    }

    *end = '\0';

    if (start != str) {
        memmove(str, start, (size_t)(end - start) + 1);
    }
}

int command_list_init(command_list_t *list) {
    if (!list) {
        return -1;
    }

    list->commands = NULL;
    list->count = 0;
    list->capacity = 0;
    return 0;
}

void command_list_free(command_list_t *list) {
    if (!list) {
        return;
    }
    free(list->commands);
    list->commands = NULL;
    list->count = 0;
    list->capacity = 0;
}

int command_list_append(command_list_t *list, const command_t *command) {
    if (!list || !command) {
        return -1;
    }

    if (list->count >= list->capacity) {
        size_t new_capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        command_t *new_commands = realloc(list->commands, new_capacity * sizeof(command_t));
        if (!new_commands) {
            return -1;
        }
        list->commands = new_commands;
        list->capacity = new_capacity;
    }

    list->commands[list->count++] = *command;
    return 0;
}

command_type_t command_type_from_string(const char *str) {
    if (!str) {
        return COMMAND_UNKNOWN;
    }

    if (strcasecmp(str, "insert") == 0) {
        return COMMAND_INSERT;
    }
    if (strcasecmp(str, "update") == 0 || strcasecmp(str, "updatesalary") == 0) {
        return COMMAND_UPDATE;
    }
    if (strcasecmp(str, "delete") == 0) {
        return COMMAND_DELETE;
    }
    if (strcasecmp(str, "search") == 0) {
        return COMMAND_SEARCH;
    }
    if (strcasecmp(str, "print") == 0) {
        return COMMAND_PRINT;
    }

    return COMMAND_UNKNOWN;
}

const char *command_type_to_string(command_type_t type) {
    switch (type) {
        case COMMAND_INSERT: return "INSERT";
        case COMMAND_UPDATE: return "UPDATE";
        case COMMAND_DELETE: return "DELETE";
        case COMMAND_SEARCH: return "SEARCH";
        case COMMAND_PRINT:  return "PRINT";
        default:             return "UNKNOWN";
    }
}

static int parse_uint32(const char *str, uint32_t *value) {
    if (!str || !value) {
        return -1;
    }

    errno = 0;
    char *endptr = NULL;
    unsigned long result = strtoul(str, &endptr, 10);
    if (errno != 0 || endptr == str) {
        return -1;
    }

    if (result > UINT32_MAX) {
        return -1;
    }

    *value = (uint32_t)result;
    return 0;
}

static int parse_int(const char *str, int *value) {
    if (!str || !value) {
        return -1;
    }

    errno = 0;
    char *endptr = NULL;
    long result = strtol(str, &endptr, 10);
    if (errno != 0 || endptr == str) {
        return -1;
    }

    *value = (int)result;
    return 0;
}

int parse_commands_file(const char *path, command_list_t *list) {
    if (!path || !list) {
        return -1;
    }

    FILE *file = fopen(path, "r");
    if (!file) {
        return -1;
    }

    command_list_init(list);

    char *line = NULL;
    size_t len = 0;
    ssize_t read = 0;

    while ((read = getline(&line, &len, file)) != -1) {
        if (read == 0) {
            continue;
        }

        if (line[read - 1] == '\n') {
            line[read - 1] = '\0';
        }

        char line_copy[256];
        strncpy(line_copy, line, sizeof(line_copy) - 1);
        line_copy[sizeof(line_copy) - 1] = '\0';
        trim_whitespace(line_copy);

        if (line_copy[0] == '\0' || line_copy[0] == '#') {
            continue;
        }

        char tokenize_buffer[256];
        strncpy(tokenize_buffer, line_copy, sizeof(tokenize_buffer) - 1);
        tokenize_buffer[sizeof(tokenize_buffer) - 1] = '\0';

        char *saveptr = NULL;
        char *token = strtok_r(tokenize_buffer, ",", &saveptr);
        if (!token) {
            continue;
        }

        trim_whitespace(token);
        command_type_t type = command_type_from_string(token);
        if (type == COMMAND_UNKNOWN) {
            continue;
        }

        command_t command = {0};
        command.type = type;
        command.state = CMD_STATE_PENDING;
        strncpy(command.raw_line, line_copy, sizeof(command.raw_line) - 1);
        command.raw_line[sizeof(command.raw_line) - 1] = '\0';

        char *name_token = NULL;
        char *salary_token = NULL;
        char *priority_token = NULL;

        switch (type) {
            case COMMAND_INSERT:
            case COMMAND_UPDATE:
                name_token = strtok_r(NULL, ",", &saveptr);
                salary_token = strtok_r(NULL, ",", &saveptr);
                priority_token = strtok_r(NULL, ",", &saveptr);
                if (!name_token || !salary_token || !priority_token) {
                    continue;
                }
                trim_whitespace(name_token);
                trim_whitespace(salary_token);
                trim_whitespace(priority_token);
                strncpy(command.name, name_token, sizeof(command.name) - 1);
                if (parse_uint32(salary_token, &command.salary) != 0) {
                    continue;
                }
                if (parse_int(priority_token, &command.priority) != 0) {
                    continue;
                }
                break;

            case COMMAND_DELETE:
            case COMMAND_SEARCH:
                name_token = strtok_r(NULL, ",", &saveptr);
                priority_token = strtok_r(NULL, ",", &saveptr);
                if (!name_token || !priority_token) {
                    continue;
                }
                trim_whitespace(name_token);
                trim_whitespace(priority_token);
                strncpy(command.name, name_token, sizeof(command.name) - 1);
                if (parse_int(priority_token, &command.priority) != 0) {
                    continue;
                }
                break;

            case COMMAND_PRINT:
                priority_token = strtok_r(NULL, ",", &saveptr);
                if (!priority_token) {
                    continue;
                }
                trim_whitespace(priority_token);
                if (parse_int(priority_token, &command.priority) != 0) {
                    continue;
                }
                break;

            default:
                continue;
        }

        if (command_list_append(list, &command) != 0) {
            free(line);
            fclose(file);
            command_list_free(list);
            return -1;
        }
    }

    free(line);
    fclose(file);
    return 0;
}


