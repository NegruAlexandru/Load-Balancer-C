/*
 * Copyright (c) 2024, Negru Alexandru
 */

#include <stdio.h>
#include <string.h>
#include "lru_cache.h"
#include "utils.h"

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
	entry *current = (*cache)->head;
	entry *next = NULL;

	while (current) {
		next = current->next;
		free(current->key);
		free(current->value);
		free(current);
		current = next;
	}

	free((*cache)->map);
	free(*cache);
	*cache = NULL;
}

void *evict_lru_entry(lru_cache *cache) {
	// Evict the LRU entry
	entry *current = cache->head;

	void *evicted_key = current->key;

	// Update the list
	if (current->next) {
		current->next->prev = NULL;
	}

	if (current->prev) {
		current->prev->next = current->next;
	}

	if (cache->tail == current) {
		cache->tail = current->prev;
	}

	cache->head = current->next;

	// Update the map
	if (current->next_hash) {
		current->next_hash->prev_hash = current->prev_hash;
	}

	if (current->prev_hash) {
		current->prev_hash->next_hash = current->next_hash;
	}

	unsigned int hash2 = hash_string(current->key) % cache->capacity;

	if (cache->map[hash2] == current) {
		cache->map[hash2] = current->next_hash;
	}

	free(current->value);
	free(current);

	cache->size--;
	return evicted_key;
}

bool lru_cache_put(lru_cache *cache, void *key, void *value,
                   void **evicted_key) {
	unsigned int hash = hash_string(key) % cache->capacity;

	if (lru_cache_is_full(cache)) {
		*evicted_key = evict_lru_entry(cache);
	} else {
		*evicted_key = NULL;
	}

	// Determine the position in the cache
	struct entry *entry = cache->map[hash];
	struct entry *prev_hash = NULL;

	// Check if the key already exists in the cache
	while (entry) {
		if (strcmp((char *)entry->key, (char *)key) == 0) {
			return false;
		}
		prev_hash = entry;
		entry = entry->next_hash;
	}

	// Create a new entry
	entry = calloc(1, sizeof(struct entry));
	DIE(entry == NULL, "calloc failed");

	entry->key = calloc(DOC_NAME_LENGTH, sizeof(char));
	DIE(entry->key == NULL, "calloc failed");
	memcpy(entry->key, key, DOC_NAME_LENGTH);

	entry->value = calloc(DOC_CONTENT_LENGTH, sizeof(char));
	DIE(entry->value == NULL, "calloc failed");
	memcpy(entry->value, value, DOC_CONTENT_LENGTH);

	entry->next = NULL;
	entry->prev = NULL;

	entry->next_hash = NULL;
	entry->prev_hash = prev_hash;

	// Update the map
	if (prev_hash) {
		prev_hash->next_hash = entry;
	} else {
		cache->map[hash] = entry;
	}

	// Update the list
	if (cache->tail) {
		cache->tail->next = entry;
		entry->prev = cache->tail;
	} else {
		cache->head = entry;
	}

	cache->tail = entry;

	cache->size++;

	return true;
}

void *lru_cache_get(lru_cache *cache, void *key) {
	unsigned int hash = hash_string(key) % cache->capacity;
	entry *entry = cache->map[hash];

	while (entry) {
		if (strcmp((char *)entry->key, (char *)key) == 0) {
			// Update the list
			if (entry != cache->tail) {
				if (entry->next) {
					entry->next->prev = entry->prev;
				}

				if (entry->prev) {
					entry->prev->next = entry->next;
				}

				if (cache->head == entry) {
					cache->head = entry->next;
				}

				entry->prev = cache->tail;
				entry->next = NULL;

				if (cache->tail) {
					cache->tail->next = entry;
				} else {
					cache->head = entry;
				}

				cache->tail = entry;
			}

			return entry->value;
		}
		entry = entry->next_hash;
	}

	return NULL;
}

void lru_cache_remove(lru_cache *cache, unsigned int hash, void *key) {
	entry *entry = cache->map[hash];
	struct entry *prev = NULL;

	while (entry) {
		if (strcmp((char *)entry->key, (char *)key) == 0) {
			// Update the map
			if (prev) {
				prev->next_hash = entry->next_hash;
			} else {
				cache->map[hash] = entry->next_hash;
			}

			if (entry->next_hash) {
				entry->next_hash->prev_hash = prev;
			}

			// Update the list
			if (entry->next) {
				entry->next->prev = entry->prev;
			}

			if (entry->prev) {
				entry->prev->next = entry->next;
			}

			if (cache->head == entry) {
				cache->head = entry->next;
			}

			if (cache->tail == entry) {
				cache->tail = entry->prev;
			}

			cache->size--;

			free(entry->key);
			free(entry->value);
			free(entry);
			return;
		}
		prev = entry;
		entry = entry->next_hash;
	}
}
