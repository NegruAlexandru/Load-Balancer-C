/*
 * Copyright (c) 2024, Negru Alexandru
 */

#include "database.h"
#include "utils.h"
#include "server.h"

void db_put(db *db, void *key, void *value) {
	entry *entry = calloc(1, sizeof(struct entry));
	DIE(entry == NULL, "calloc failed");

	entry->key = calloc(DOC_NAME_LENGTH, sizeof(char));
	DIE(entry->key == NULL, "calloc failed");
	memcpy(entry->key, key, DOC_NAME_LENGTH);

	entry->value = calloc(DOC_CONTENT_LENGTH, sizeof(char));
	DIE(entry->value == NULL, "calloc failed");
	memcpy(entry->value, value, DOC_CONTENT_LENGTH);

	unsigned int hash = hash_string(key) % db->capacity;
	entry->next_hash = db->map[hash];
	db->map[hash] = entry;

	db->size++;
}

void *db_get(db *db, void *key) {
	unsigned int hash = hash_string(key) % db->capacity;
	entry *entry = db->map[hash];

	while (entry != NULL) {
		if (entry->key) {
			if (strcmp((char *)entry->key, (char *)key) == 0) {
				return entry->value;
			}
		}
		entry = entry->next_hash;
	}

	return NULL;
}

void db_remove(db *db, unsigned int hash, void *key) {
	entry *entry = db->map[hash];
	struct entry *prev = NULL;

	while (entry) {
		if (strcmp((char *)entry->key, (char *)key) == 0) {
			if (prev) {
				prev->next_hash = entry->next_hash;
			} else {
				db->map[hash] = entry->next_hash;
			}

			if (entry->next_hash) {
				entry->next_hash->prev_hash = prev;
			}

			db->size--;

			free(entry->key);
			free(entry->value);
			free(entry);
			return;
		}
		prev = entry;
		entry = entry->next_hash;
	}
}

void free_db(db **db) {
	for (unsigned int i = 0; i < (*db)->capacity; i++) {
		entry *entry = (*db)->map[i];
		while (entry != NULL) {
			struct entry *next = entry->next_hash;
			free(entry->key);
			free(entry->value);
			free(entry);
			entry = next;
		}
	}
	free((*db)->map);
	free(*db);
	*db = NULL;
}
