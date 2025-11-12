#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stddef.h>
#include <stdint.h>

#define HASH_TABLE_SIZE 1024
#define HASH_NAME_MAX 64

typedef struct hash_record {
    uint32_t hash;
    char name[HASH_NAME_MAX];
    uint32_t salary;
    struct hash_record *next;
} hash_record_t;

typedef struct hash_table {
    hash_record_t *buckets[HASH_TABLE_SIZE];
} hash_table_t;

typedef enum {
    HASH_INSERTED = 0,
    HASH_UPDATED = 1,
    HASH_NOOP = 2
} hash_insert_status_t;

void hash_table_init(hash_table_t *table);

hash_insert_status_t hash_table_insert(hash_table_t *table,
                                       const char *name,
                                       uint32_t salary,
                                       hash_record_t *out_record,
                                       uint32_t *out_previous_salary);

int hash_table_update(hash_table_t *table,
                      const char *name,
                      uint32_t salary,
                      hash_record_t *out_record,
                      uint32_t *out_previous_salary);

int hash_table_delete(hash_table_t *table,
                      const char *name,
                      hash_record_t *out_record);

int hash_table_search(hash_table_t *table,
                      const char *name,
                      hash_record_t *out_record);

hash_record_t *hash_table_snapshot(const hash_table_t *table, size_t *out_count);

void hash_table_free_snapshot(hash_record_t *records, size_t count);

uint32_t jenkins_one_at_a_time_hash(const char *key);

#endif /* HASH_TABLE_H */


