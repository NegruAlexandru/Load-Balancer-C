/*
 * Copyright (c) 2024, Negru Alexandru
 */

#ifndef DATABASE_H
#define DATABASE_H

#include "server.h"

/**
 * @brief Puts a key-value pair in the database.
 *
 * @param db: Database where the key-value pair will be stored.
 * @param key: Key of the pair.
 * @param value: Value of the pair.
 */
void db_put(db *db, void *key, void *value);

/**
 * @brief Retrieves the value associated with a key.
 *
 * @param db: Database where the key-value pair is stored.
 * @param key: Key of the pair.
 *
 * @return - The value associated with the key,
 *      or NULL if the key is not found.
 */
void *db_get(db *db, void *key);

/**
 * @brief Removes a key-value pair from the database.
 *
 * @param db: Database where the key-value pair is stored.
 * @param hash: Hash of the key.
 * @param key: Key of the pair.
 */
void db_remove(db *db, unsigned int hash, void *key);

/**
 * @brief Frees the database.
 *
 * @param db: Database to be freed.
 */
void free_db(db **db);
#endif
