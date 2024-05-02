/*
 * Copyright (c) 2024, <>
 */

#include "load_balancer.h"
#include "server.h"

load_balancer *init_load_balancer(bool enable_vnodes) {
	load_balancer *main = calloc(1, sizeof(load_balancer));
	DIE(main == NULL, "calloc failed");

	main->enable_vnodes = enable_vnodes;
	main->servers_count = 0;

	return main;
}

void loader_add_server(load_balancer* main, unsigned int server_id, unsigned int cache_size) {
	// Add server
	main->servers[main->servers_count] = init_server(server_id, cache_size);
	main->servers_count++;

	// Migrate documents
	if (main->servers_count == 1) {
		return;
	} else {
		// migrate documents
	}
}

void loader_remove_server(load_balancer* main, unsigned int server_id) {
	// Migrate documents
	if (main->servers_count == 1) {
		return;
	} else {
		// migrate documents
	}

	// Remove server
	free_server(&main->servers[main->servers_count - 1]);
	main->servers_count--;
}

response *loader_forward_request(load_balancer* main, request *req) {
	unsigned int doc_hash = hash_string(req->doc_name);
	for (int i = 0; i < main->servers_count; i++) {
		if (main->servers[i]->hash_ring_position >= doc_hash) {
			return server_handle_request(main->servers[i], req);
		}
	}

	return server_handle_request(main->servers[0], req);
}

void free_load_balancer(load_balancer** main) {
	for (int i = 0; i < (*main)->servers_count; i++) {
		free_server(&(*main)->servers[i]);
	}
    free(*main);

    *main = NULL;
}
