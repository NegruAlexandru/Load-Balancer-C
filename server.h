/*
 * Copyright (c) 2024, Negru Alexandru
 */

#ifndef SERVER_H
#define SERVER_H

#include "utils.h"
#include "constants.h"
#include "lru_cache.h"

#define TASK_QUEUE_SIZE         1000
#define MAX_LOG_LENGTH          1000
#define MAX_RESPONSE_LENGTH     4096
#define MAX_DB_BUCKETS			1000

typedef struct request {
	request_type type;
	char *doc_name;
	char *doc_content;
} request;

typedef struct response {
	char *server_log;
	char *server_response;
	unsigned int server_id;
} response;

typedef struct request_queue {
	request **requests;
	unsigned int size;
	unsigned int capacity;
} request_queue;

typedef struct db {
	unsigned int size;
	unsigned int capacity;
	entry **map;
} db;

typedef struct server {
	unsigned int server_id;
	unsigned int hash_ring_position;
	request_queue *request_queue;
	lru_cache *cache;
	db *db;
} server;

/**
 * init_server() - Initializes a server with the given server_id and
 * 				cache_size.
 *
 * @param server_id: ID of the server.
 * @param cache_size: Size of the cache.
 *
 * @return server*: The newly created server.
 */

server *init_server(unsigned int server_id, unsigned int cache_size);

/**
 * @brief Deallocates completely the memory used by server,
 *     taking care of deallocating the elements in the queue, if any,
 *     without executing the tasks
 */
void free_server(server **s);

/**
 * free_virtual_server() - Deallocates the memory used by a
 *			virtual server.
 * @param s: Server to be deallocated.
 */

void free_virtual_server(server **s);

/**
 * server_handle_request() - Receives a request from the load balancer
 *      and processes it according to the request type
 * 
 * @param s: Server which processes the request.
 * @param req: Request to be processed.
 * 
 * @return response*: Response of the requested operation, which will
 *      then be printed in main.
 * 
 * @brief Based on the type of request, should call the appropriate
 *     solver, and should execute the tasks from queue if needed (in
 *     this case, after executing each task, PRINT_RESPONSE should
 *     be called).
 */
response *server_handle_request(server *s, request *req);

/**
 * server_enqueue_request() - Adds a request to the server's queue.
 *
 * @param server: Server which receives the request.
 * @param req: Request to be added to the queue.
 * @param make_response: If true, the server should execute the request
 *      and return the response, otherwise, the server should only add
 *      the request to the queue.
 *
 * @return response*: Response of the requested operation, if make_response
 *      is true, otherwise NULL.
 *
 * @brief Adds the request to the queue and, if make_response is true,
 *     wil return the response (used for GET requests).
 */
response *server_enqueue_request(server *server, request *req,
								 bool make_response);

/**
 * server_execute_all_requests() - Executes all the requests in the queue.
 *
 * @param server: Server which executes the requests.
 *
 * @return response*: Response of the GET request or NULL.
 *
 * @brief Executes all the requests in the queue until it finds a GET
 *    request, then returns the response of that request.
 */
response *server_execute_all_requests(server *server);

#endif  /* SERVER_H */
