//
// Created by Alex on 29/04/2024.
//

#include "database.h"
#include "utils.h"
#include "server.h"

void db_put(db *db, void *key, void *value) {
	entry *entry = calloc(1, sizeof(entry));
	DIE(entry == NULL, "calloc failed");

	entry->key = key;
	entry->value = value;

	unsigned int hash = hash_uint(key) % db->capacity;
	entry->next = db->map[hash];
	db->map[hash] = entry;

	db->size++;
}

void *db_get(db *db, void *key) {
	unsigned int hash = hash_uint(key) % db->capacity;
	entry *entry = db->map[hash];

	while (entry != NULL) {
		if (entry->key == key) {
			return entry->value;
		}
		entry = entry->next;
	}

	return NULL;
}

void free_db(db **db) {
	for (int i = 0; i < (*db)->capacity; i++) {
		entry *entry = (*db)->map[i];
		while (entry != NULL) {
			struct entry *next = entry->next;
			free(entry);
			entry = next;
		}
	}
	free((*db)->map);
	free(*db);
	*db = NULL;
}