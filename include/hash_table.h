#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define MAX_NAME_LEN 50

typedef struct hash_record {
    uint32_t hash;
    char name[MAX_NAME_LEN];
    uint32_t salary;
    struct hash_record *next;
} hash_record_t;

typedef struct {
    hash_record_t *head;
} hash_table_t;

typedef struct {
    uint32_t hash;
    char name[MAX_NAME_LEN];
    uint32_t salary;
} record_snapshot_t;

typedef enum {
    TABLE_OK = 0,
    TABLE_DUPLICATE,
    TABLE_NOT_FOUND
} table_status_t;

void hash_table_init(hash_table_t *table);
void hash_table_destroy(hash_table_t *table);
table_status_t hash_table_insert(hash_table_t *table, uint32_t hash,
                                 const char *name, uint32_t salary);
table_status_t hash_table_update(hash_table_t *table, uint32_t hash,
                                 uint32_t salary, record_snapshot_t *before,
                                 record_snapshot_t *after);
table_status_t hash_table_delete(hash_table_t *table, uint32_t hash,
                                 record_snapshot_t *removed);
bool hash_table_find(const hash_table_t *table, uint32_t hash,
                     record_snapshot_t *result);
// Returns the number of copied records. If allocation fails, returns SIZE_MAX.
size_t hash_table_snapshot(const hash_table_t *table,
                           record_snapshot_t **records_out);

#endif
