//
// Created by Alex on 29/04/2024.
//

#ifndef DATABASE_H
#define DATABASE_H

#include "server.h"

void db_put(db *db, void *key, void *value);
void *db_get(db *db, void *key);
void free_db(db **db);
#endif
