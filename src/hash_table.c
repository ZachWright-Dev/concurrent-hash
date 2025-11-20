#include "hash_table.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static hash_record_t *create_record(uint32_t hash, const char *name,
                                    uint32_t salary) {
    hash_record_t *record = (hash_record_t *)malloc(sizeof(hash_record_t));
    if (!record) {
        return NULL;
    }

    record->hash = hash;
    strncpy(record->name, name, MAX_NAME_LEN - 1);
    record->name[MAX_NAME_LEN - 1] = '\0';
    record->salary = salary;
    record->next = NULL;
    return record;
}

void hash_table_init(hash_table_t *table) {
    table->head = NULL;
}

void hash_table_destroy(hash_table_t *table) {
    hash_record_t *current = table->head;
    while (current) {
        hash_record_t *next = current->next;
        free(current);
        current = next;
    }
    table->head = NULL;
}

table_status_t hash_table_insert(hash_table_t *table, uint32_t hash,
                                 const char *name, uint32_t salary) {
    hash_record_t *prev = NULL;
    hash_record_t *current = table->head;

    while (current && current->hash < hash) {
        prev = current;
        current = current->next;
    }

    if (current && current->hash == hash) {
        return TABLE_DUPLICATE;
    }

    hash_record_t *record = create_record(hash, name, salary);
    if (!record) {
        return TABLE_NOT_FOUND;  // reuse error for allocation failure
    }

    record->next = current;
    if (prev) {
        prev->next = record;
    } else {
        table->head = record;
    }

    return TABLE_OK;
}

table_status_t hash_table_update(hash_table_t *table, uint32_t hash,
                                 uint32_t salary, record_snapshot_t *before,
                                 record_snapshot_t *after) {
    hash_record_t *current = table->head;
    while (current && current->hash < hash) {
        current = current->next;
    }

    if (!current || current->hash != hash) {
        return TABLE_NOT_FOUND;
    }

    if (before) {
        before->hash = current->hash;
        strncpy(before->name, current->name, MAX_NAME_LEN);
        before->salary = current->salary;
    }

    current->salary = salary;

    if (after) {
        after->hash = current->hash;
        strncpy(after->name, current->name, MAX_NAME_LEN);
        after->salary = current->salary;
    }

    return TABLE_OK;
}

table_status_t hash_table_delete(hash_table_t *table, uint32_t hash,
                                 record_snapshot_t *removed) {
    hash_record_t *prev = NULL;
    hash_record_t *current = table->head;

    while (current && current->hash < hash) {
        prev = current;
        current = current->next;
    }

    if (!current || current->hash != hash) {
        return TABLE_NOT_FOUND;
    }

    if (prev) {
        prev->next = current->next;
    } else {
        table->head = current->next;
    }

    if (removed) {
        removed->hash = current->hash;
        strncpy(removed->name, current->name, MAX_NAME_LEN);
        removed->salary = current->salary;
    }

    free(current);
    return TABLE_OK;
}

bool hash_table_find(const hash_table_t *table, uint32_t hash,
                     record_snapshot_t *result) {
    hash_record_t *current = table->head;
    while (current && current->hash < hash) {
        current = current->next;
    }

    if (!current || current->hash != hash) {
        return false;
    }

    if (result) {
        result->hash = current->hash;
        strncpy(result->name, current->name, MAX_NAME_LEN);
        result->salary = current->salary;
    }

    return true;
}

size_t hash_table_snapshot(const hash_table_t *table,
                           record_snapshot_t **records_out) {
    size_t count = 0;
    hash_record_t *current = table->head;
    while (current) {
        count++;
        current = current->next;
    }

    record_snapshot_t *records = NULL;
    if (count > 0) {
        records = (record_snapshot_t *)calloc(count, sizeof(record_snapshot_t));
        if (!records) {
            *records_out = NULL;
            return SIZE_MAX;
        }
    }

    current = table->head;
    for (size_t i = 0; i < count; ++i) {
        records[i].hash = current->hash;
        strncpy(records[i].name, current->name, MAX_NAME_LEN);
        records[i].salary = current->salary;
        current = current->next;
    }

    *records_out = records;
    return count;
}
