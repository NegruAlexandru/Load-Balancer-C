/*
 * Copyright (c) 2024, <>
 */

#include <stdio.h>
#include "server.h"
#include "lru_cache.h"
#include "database.h"
#include "utils.h"

static response
*server_edit_document(server *s, char *doc_name, char *doc_content) {
	response *response = calloc(1, sizeof(response));
	DIE(response == NULL, "calloc failed");
	response->server_id = s->server_id;

	void *value = lru_cache_get(s->cache, doc_name);
	if (value) {
		// Update the cache
		memcpy(value, doc_content, strlen(doc_content));

		// Update the database
		value = db_get(s->db, doc_name);
		memcpy(value, doc_content, strlen(doc_content));

		// Server response + log
		memcpy(response->server_response, MSG_B, strlen(MSG_B));
		memcpy(response->server_log, LOG_HIT, strlen(LOG_HIT));
	} else {
		value = db_get(s->db, doc_name);

		if (value) {
			// Update the database
			memcpy(value, doc_content, strlen(doc_content));

			// Server log
			log_resp_for_cache_full_state(response, s->cache);

			// New entry in cache
			lru_cache_put(s->cache, doc_name, doc_content, NULL);

			// Server response
			memcpy(response->server_response, MSG_B, strlen(MSG_B));
		} else {
			// New entry in database
			db_put(s->db, doc_name, doc_content);

			// Server log
			log_resp_for_cache_full_state(response, s->cache);

			// New entry in cache
			lru_cache_put(s->cache, doc_name, doc_content, NULL);

			// Server response
			memcpy(response->server_response, MSG_C, strlen(MSG_C));
		}
	}

	return response;
}

void log_resp_for_cache_full_state(response *response, lru_cache *cache) {
	if (lru_cache_is_full(cache)) {
		memcpy(response->server_log, LOG_EVICT, strlen(LOG_EVICT));
	} else {
		memcpy(response->server_log, LOG_MISS, strlen(LOG_MISS));
	}
}

static response
*server_get_document(server *s, char *doc_name) {
	response *response = calloc(1, sizeof(response));
	DIE(response == NULL, "calloc failed");
	response->server_id = s->server_id;

	void *value = lru_cache_get(s->cache, doc_name);
	if (value) {
		// Server response + log
		memcpy(response->server_response, value, strlen(value));
		memcpy(response->server_log, LOG_HIT, strlen(LOG_HIT));
	} else {
		value = db_get(s->db, doc_name);

		if (value) {
			// Server log
			log_resp_for_cache_full_state(response, s->cache);

			// New entry in cache
			lru_cache_put(s->cache, doc_name, value, NULL);

			// Server response
			memcpy(response->server_response, value, strlen(value));
		} else {
			// Server log
			log_resp_for_cache_full_state(response, s->cache);

			// Server response
			memcpy(response->server_response, MSG_A, strlen(MSG_A));
		}
	}

	return response;
}

server *init_server(unsigned int server_id, unsigned int cache_size) {
    server *server = calloc(1, sizeof(server));
	DIE(server == NULL, "calloc failed");

	server->server_id = server_id;
	server->hash_ring_position = hash_uint((void *) server_id);

	server->cache = init_lru_cache(cache_size);

	server->db = calloc(1, sizeof(db));
	DIE(server->db == NULL, "calloc failed");
	server->db->capacity = MAX_DB_BUCKETS;
	server->db->size = 0;

	server->request_queue = calloc(1, sizeof(request_queue));
	DIE(server->request_queue == NULL, "calloc failed");
	server->request_queue->capacity = TASK_QUEUE_SIZE;
	server->request_queue->size = 0;

	return server;
}

static response *
enqueue_request(server *server, request *req) {
	request_queue *queue = server->request_queue;
	if (queue->size < queue->capacity) {
		queue->requests[queue->size] = req;
		queue->size++;
	}

	response *response = calloc(1, sizeof(response));
	DIE(response == NULL, "calloc failed");
	response->server_id = server->server_id;

	memcpy(response->server_response, MSG_A, strlen(MSG_A));

	return response;
}

static void
server_execute_all_requests(server *s) {
	for (int i = 0; i < s->request_queue->size; i++) {
		request *req = s->request_queue->requests[i];
		response *response = NULL;

		switch (req->type) {
			case EDIT_DOCUMENT:
				response = server_edit_document(s, req->doc_name, req->doc_content);
				break;
			case GET_DOCUMENT:
				response = server_get_document(s, req->doc_name);
				break;
			default:
				break;
		}

		if (response) {
			PRINT_RESPONSE(response);
		}
	}
}

response *server_handle_request(server *s, request *req) {
	response *response = NULL;

	switch (req->type) {
	case EDIT_DOCUMENT:
		enqueue_request(s, req);
		break;
	case GET_DOCUMENT:
		enqueue_request(s, req);
		server_execute_all_requests(s);
		break;
	default:
		break;
	}

	return response;

}

void free_server(server **s) {
	free_lru_cache(&(*s)->cache);
	free_db(&(*s)->db);
	free((*s)->request_queue);
	free(*s);
	*s = NULL;
}
