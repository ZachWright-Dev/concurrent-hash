#include "hash_table.h"

#include <stdlib.h>
#include <string.h>

static uint32_t bucket_index(uint32_t hash) {
    return hash % HASH_TABLE_SIZE;
}

void hash_table_init(hash_table_t *table) {
    if (!table) {
        return;
    }

    memset(table->buckets, 0, sizeof(table->buckets));
}

hash_insert_status_t hash_table_insert(hash_table_t *table,
                                       const char *name,
                                       uint32_t salary,
                                       hash_record_t *out_record,
                                       uint32_t *out_previous_salary) {
    if (!table || !name) {
        return HASH_NOOP;
    }

    uint32_t hash = jenkins_one_at_a_time_hash(name);
    uint32_t index = bucket_index(hash);

    hash_record_t *current = table->buckets[index];
    while (current) {
        if (strcmp(current->name, name) == 0) {
            if (out_record) {
                *out_record = *current;
            }
            if (out_previous_salary) {
                *out_previous_salary = current->salary;
            }
            current->salary = salary;
            return HASH_UPDATED;
        }
        current = current->next;
    }

    hash_record_t *node = calloc(1, sizeof(hash_record_t));
    if (!node) {
        return HASH_NOOP;
    }

    node->hash = hash;
    strncpy(node->name, name, HASH_NAME_MAX - 1);
    node->name[HASH_NAME_MAX - 1] = '\0';
    node->salary = salary;
    node->next = table->buckets[index];
    table->buckets[index] = node;

    if (out_record) {
        *out_record = *node;
    }

    if (out_previous_salary) {
        *out_previous_salary = 0;
    }

    return HASH_INSERTED;
}

int hash_table_update(hash_table_t *table,
                      const char *name,
                      uint32_t salary,
                      hash_record_t *out_record,
                      uint32_t *out_previous_salary) {
    if (!table || !name) {
        return 0;
    }

    uint32_t hash = jenkins_one_at_a_time_hash(name);
    uint32_t index = bucket_index(hash);

    hash_record_t *current = table->buckets[index];
    while (current) {
        if (strcmp(current->name, name) == 0) {
            uint32_t prev = current->salary;
            current->salary = salary;
            current->hash = hash;

            if (out_record) {
                *out_record = *current;
            }

            if (out_previous_salary) {
                *out_previous_salary = prev;
            }

            return 1;
        }
        current = current->next;
    }

    return 0;
}

int hash_table_delete(hash_table_t *table,
                      const char *name,
                      hash_record_t *out_record) {
    if (!table || !name) {
        return 0;
    }

    uint32_t hash = jenkins_one_at_a_time_hash(name);
    uint32_t index = bucket_index(hash);

    hash_record_t *current = table->buckets[index];
    hash_record_t *previous = NULL;

    while (current) {
        if (strcmp(current->name, name) == 0) {
            if (previous) {
                previous->next = current->next;
            } else {
                table->buckets[index] = current->next;
            }

            if (out_record) {
                *out_record = *current;
            }

            free(current);
            return 1;
        }

        previous = current;
        current = current->next;
    }

    return 0;
}

int hash_table_search(hash_table_t *table,
                      const char *name,
                      hash_record_t *out_record) {
    if (!table || !name) {
        return 0;
    }

    uint32_t hash = jenkins_one_at_a_time_hash(name);
    uint32_t index = bucket_index(hash);

    hash_record_t *current = table->buckets[index];
    while (current) {
        if (strcmp(current->name, name) == 0) {
            if (out_record) {
                *out_record = *current;
            }
            return 1;
        }
        current = current->next;
    }

    return 0;
}

static int compare_records(const void *a, const void *b) {
    const hash_record_t *ra = (const hash_record_t *)a;
    const hash_record_t *rb = (const hash_record_t *)b;

    if (ra->hash < rb->hash) {
        return -1;
    }
    if (ra->hash > rb->hash) {
        return 1;
    }
    return strcmp(ra->name, rb->name);
}

hash_record_t *hash_table_snapshot(const hash_table_t *table, size_t *out_count) {
    if (!table || !out_count) {
        return NULL;
    }

    size_t count = 0;
    for (size_t i = 0; i < HASH_TABLE_SIZE; ++i) {
        hash_record_t *current = table->buckets[i];
        while (current) {
            ++count;
            current = current->next;
        }
    }

    if (count == 0) {
        *out_count = 0;
        return NULL;
    }

    hash_record_t *records = calloc(count, sizeof(hash_record_t));
    if (!records) {
        *out_count = 0;
        return NULL;
    }

    size_t idx = 0;
    for (size_t i = 0; i < HASH_TABLE_SIZE; ++i) {
        hash_record_t *current = table->buckets[i];
        while (current) {
            records[idx++] = *current;
            current = current->next;
        }
    }

    qsort(records, count, sizeof(hash_record_t), compare_records);

    *out_count = count;
    return records;
}

void hash_table_free_snapshot(hash_record_t *records, size_t count) {
    (void)count;
    free(records);
}

uint32_t jenkins_one_at_a_time_hash(const char *key) {
    uint32_t hash = 0;

    for (const unsigned char *ptr = (const unsigned char *)key; *ptr; ++ptr) {
        hash += *ptr;
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }

    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);

    return hash;
}


