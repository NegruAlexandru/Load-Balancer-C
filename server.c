/*
 * Copyright (c) 2024, Negru Alexandru
 */

#include <stdio.h>
#include "server.h"
#include "lru_cache.h"
#include "database.h"
#include "utils.h"

response *create_response(server *s);

static response
*server_edit_document(server *s, char *doc_name, char *doc_content) {
	// Allocate response memory
	response *resp = create_response(s);

	// Key of the evicted entry
	void *evicted_key = NULL;

	// Get the value from the cache
	void *value = lru_cache_get(s->cache, doc_name);

	// If the document is in the cache
	if (value) {
		// Update the cache
		memset(value, 0, DOC_CONTENT_LENGTH);
		memcpy(value, doc_content, strlen(doc_content));

		// Update the database
		value = db_get(s->db, doc_name);
		if (value) {
			memset(value, 0, DOC_CONTENT_LENGTH);
			memcpy(value, doc_content, strlen(doc_content));
		} else {
			db_put(s->db, doc_name, doc_content);
		}

		// Server resp + log
		sprintf(resp->server_response, MSG_B, doc_name);
		sprintf(resp->server_log, LOG_HIT, doc_name);
	} else {
		// Get the value from the database
		value = db_get(s->db, doc_name);

		// If the document is in the database
		if (value) {
			// Update the database
			memset(value, 0, DOC_CONTENT_LENGTH);
			memcpy(value, doc_content, strlen(doc_content));

			// Add entry in cache
			lru_cache_put(s->cache, doc_name, doc_content, &evicted_key);

			// Server log
			if (evicted_key) {
				sprintf(resp->server_log, LOG_EVICT, doc_name,
						(char *)evicted_key);
			} else {
				sprintf(resp->server_log, LOG_MISS, doc_name);
			}

			// Server resp
			sprintf(resp->server_response, MSG_B, doc_name);
		} else {
			// Add entry in database
			db_put(s->db, doc_name, doc_content);

			// Add entry in cache
			lru_cache_put(s->cache, doc_name, doc_content, &evicted_key);

			// Server log
			if (evicted_key) {
				sprintf(resp->server_log, LOG_EVICT, doc_name,
						(char *)evicted_key);
			} else {
				sprintf(resp->server_log, LOG_MISS, doc_name);
			}

			// Server resp
			sprintf(resp->server_response, MSG_C, doc_name);
		}
	}

	// Free the evicted key
	if (evicted_key) {
		free(evicted_key);
	}

	return resp;
}

static response
*server_get_document(server *s, char *doc_name) {
	// Allocate response memory
	response *resp = create_response(s);

	// Key of the evicted entry
	void *evicted_key = NULL;

	// Get the value from the cache
	void *value = lru_cache_get(s->cache, doc_name);

	// If the document is in the cache
	if (value) {
		// Server resp + log
		sprintf(resp->server_response, "%s", (char *)value);
		sprintf(resp->server_log, LOG_HIT, doc_name);
	} else {
		// Get the value from the database
		value = db_get(s->db, doc_name);

		// If the document is in the database
		if (value) {
			// Server resp
			sprintf(resp->server_response, "%s", (char *)value);

			// New entry in cache
			lru_cache_put(s->cache, doc_name, value, &evicted_key);

			// Server log
			if (evicted_key) {
				sprintf(resp->server_log, LOG_EVICT, doc_name,
						(char *)evicted_key);
			} else {
				sprintf(resp->server_log, LOG_MISS, doc_name);
			}
		} else {
			// Server resp
			sprintf(resp->server_response, "(null)");

			// Server log
			sprintf(resp->server_log, LOG_FAULT, doc_name);
		}
	}

	// Free the evicted key
	if (evicted_key) {
		free(evicted_key);
	}

	return resp;
}

server *init_server(unsigned int server_id, unsigned int cache_size) {
	// Allocate server memory
    server *s = malloc(sizeof(server));
	DIE(s == NULL, "malloc failed");

	// Initialize server id and hash ring position
	s->server_id = server_id;
	s->hash_ring_position = hash_uint(&server_id);

	// Initialize the LRU cache
	s->cache = init_lru_cache(cache_size);

	// Initialize the database
	s->db = malloc(sizeof(db));
	DIE(s->db == NULL, "calloc failed");

	s->db->map = calloc(MAX_DB_BUCKETS, sizeof(entry *));
	DIE(s->db->map == NULL, "calloc failed");

	s->db->capacity = MAX_DB_BUCKETS;
	s->db->size = 0;

	// Initialize the request queue
	s->request_queue = malloc(sizeof(request_queue));
	DIE(s->request_queue == NULL, "calloc failed");

	s->request_queue->requests = calloc(TASK_QUEUE_SIZE, sizeof(request *));
	DIE(s->request_queue->requests == NULL, "calloc failed");

	s->request_queue->capacity = TASK_QUEUE_SIZE;
	s->request_queue->size = 0;

	return s;
}

response *
server_enqueue_request(server *server, request *req, bool make_response) {
	// Add the request to the queue
	request_queue *queue = server->request_queue;
	if (queue->size < queue->capacity) {
		// Allocate request memory
		request *request = calloc(1, sizeof(struct request));
		DIE(request == NULL, "calloc failed");

		request->type = req->type;

		// Copy the document name
		request->doc_name = calloc(DOC_NAME_LENGTH, sizeof(char));
		DIE(request->doc_name == NULL, "calloc failed");

		memcpy(request->doc_name, req->doc_name, strlen(req->doc_name));

		// Copy the document content if it exists
		if (req->doc_content) {
			request->doc_content = calloc(DOC_CONTENT_LENGTH, sizeof(char));
			memcpy(request->doc_content, req->doc_content,
				   strlen(req->doc_content));
			DIE(request->doc_content == NULL, "calloc failed");
		} else {
			request->doc_content = NULL;
		}

		// Add the request to the queue
		queue->requests[queue->size] = request;
		queue->size++;
	} else {
		// If the queue is full, execute all requests
		server_execute_all_requests(server);
		server_enqueue_request(server, req, make_response);
	}

	// If the request is an edit request, make the response
	if (make_response) {
		// Allocate response memory
		response *resp = create_response(server);

		sprintf(resp->server_response, MSG_A, "EDIT",
				req->doc_name);
		sprintf(resp->server_log, LOG_LAZY_EXEC, queue->size);

		return resp;
	}

	return NULL;
}

response *
server_execute_all_requests(server *s) {
	// Get the request queue
	request_queue *queue = s->request_queue;
	int i = 0;

	// Execute all requests
	while (queue->size > 0) {
		request *req = queue->requests[i];
		response *resp = NULL;

		// Execute the request based on the type
		switch (req->type) {
			case EDIT_DOCUMENT:
				// Execute the request and print the response
				resp = server_edit_document(s, req->doc_name,
											req->doc_content);
				free(req->doc_name);
				free(req->doc_content);
				free(req);
				PRINT_RESPONSE(resp);
				break;
			case GET_DOCUMENT:
				// Execute the request and return the response
				resp = server_get_document(s, req->doc_name);
				free(req->doc_name);
				free(req->doc_content);
				free(req);
				queue->size = 0;
				return resp;
			default:
				break;
		}

		i++;
		queue->size--;
	}

	return NULL;
}

response *server_handle_request(server *s, request *req) {
	response *resp = NULL;
	bool make_response;

	// Handle the request based on the type
	switch (req->type) {
	case EDIT_DOCUMENT:
		// Add the request to the queue
		make_response = true;
		resp = server_enqueue_request(s, req, make_response);
		break;
	case GET_DOCUMENT:
		// Add the request to the queue, then execute all requests in the queue
		make_response = false;
		server_enqueue_request(s, req, make_response);
		resp = server_execute_all_requests(s);
		break;
	default:
		break;
	}

	return resp;
}

void free_server(server **s) {
	free_lru_cache(&(*s)->cache);
	free_db(&(*s)->db);
	for (unsigned int i = 0; i < (*s)->request_queue->size; i++) {
		free((*s)->request_queue->requests[i]->doc_name);
		free((*s)->request_queue->requests[i]->doc_content);
		free((*s)->request_queue->requests[i]);
	}
	free((*s)->request_queue->requests);
	free((*s)->request_queue);
	free(*s);
	*s = NULL;
}

void free_virtual_server(server **s) {
	free(*s);
	*s = NULL;
}

response *create_response(server *s) {
	response *resp = calloc(1, sizeof(response));
	DIE(resp == NULL, "calloc failed");

	resp->server_response = calloc(MAX_RESPONSE_LENGTH, sizeof(char));
	DIE(resp->server_response == NULL, "calloc failed");

	resp->server_log = calloc(MAX_LOG_LENGTH, sizeof(char));
	DIE(resp->server_log == NULL, "calloc failed");

	resp->server_id = s->server_id;

	return resp;
}
