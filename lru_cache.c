/*
 * Copyright (c) 2024, <>
 */

#include <stdio.h>
#include <string.h>
#include "lru_cache.h"
#include "utils.h"

typedef struct entry lru_cache_entry;

lru_cache *init_lru_cache(unsigned int cache_capacity) {
	lru_cache *cache = calloc(1, sizeof(lru_cache));
	DIE(cache == NULL, "calloc failed");

	cache->capacity = cache_capacity;
	cache->size = 0;
	cache->head = NULL;
	cache->tail = NULL;
	cache->map = calloc(cache_capacity, sizeof(entry *));
	DIE(cache->map == NULL, "calloc failed");

	return cache;
}

bool lru_cache_is_full(lru_cache *cache) {
	return cache->size == cache->capacity;
}

void free_lru_cache(lru_cache **cache) {

	for (int i = 0; i < (*cache)->capacity; i++) {
		entry *entry = (*cache)->map[i];
		while (entry != NULL) {
			struct entry *next = entry->next;
			free(entry);
			entry = next;
		}
	}
	free((*cache)->map);
	free(*cache);
	*cache = NULL;
}

bool lru_cache_put(lru_cache *cache, void *key, void *value,
                   void **evicted_key) {

	// Determine the position in the cache
	unsigned int hash = hash_uint(key) % cache->capacity;
	struct entry *entry = cache->map[hash];
	struct entry *prev = NULL;

	// Check if the key already exists in the cache
	while (entry != NULL) {
		if (entry->key == key) {
			return false;
		}
		prev = entry;
		entry = entry->next;
	}

	// Create a new entry
	entry = calloc(1, sizeof(entry));
	DIE(entry == NULL, "calloc failed");
	entry->key = key;
	entry->value = value;
	entry->next = NULL;
	entry->prev = prev;
	entry->hash_ring_position = hash;

	// Add the entry to the cache
	if (prev == NULL) {
		cache->map[hash] = entry;
	} else {
		prev->next = entry;
	}

	// Update the LRU list
	if (cache->head == NULL) {
		cache->head = entry;
		cache->tail = entry;
	} else {
		cache->tail->next = entry;
		cache->tail = entry;
	}

	cache->size++;

	if (lru_cache_is_full(cache)) {
		*evicted_key = cache->head->key;
		struct entry *next = cache->head->next;
		free(cache->head);
		cache->head = next;
		next->prev = NULL;
		cache->size--;
	}

	return true;
}

void *lru_cache_get(lru_cache *cache, void *key) {
	unsigned int hash = hash_string(key) % cache->capacity;
	entry *entry = cache->map[hash];

	while (entry != NULL) {
		if (entry->key == key) {
			return entry->value;
		}
		entry = entry->next;
	}

	return NULL;
}

void lru_cache_remove(lru_cache *cache, void *key) {
	unsigned int hash = hash_uint(key) % cache->capacity;
	struct entry *entry = cache->map[hash];
	struct entry * prev = cache->map[hash];

	while (entry != NULL) {
		if (entry->key == key) {
			if (prev == NULL) {
				cache->map[hash] = entry->next;
			} else {
				prev->next = entry->next;
			}
			free(entry);
			cache->size--;
			return;
		}
		prev = entry;
		entry = entry->next;
	}
}
