/*
 * Copyright (c) 2024, Negru Alexandru
 */

#include "load_balancer.h"
#include "server.h"
#include "utils.h"
#include "database.h"

load_balancer *init_load_balancer(bool enable_vnodes) {
	// Allocate memory for the load balancer
	load_balancer *main = calloc(1, sizeof(load_balancer));
	DIE(main == NULL, "calloc failed");

	// Initialize the load balancer fields
	main->hash_function_servers = hash_uint;
	main->hash_function_docs = hash_string;
	main->enable_vnodes = enable_vnodes;
	main->servers_count = 0;

	return main;
}

void loader_add_server(load_balancer* main, unsigned int server_id,
					   unsigned int cache_size) {
	// Check if the load balancer has virtual nodes
	if (main->enable_vnodes) {
		loader_add_server_vnodes(main, server_id, cache_size);
	} else {
		loader_add_server_no_vnodes(main, server_id, cache_size);
	}
}

void loader_remove_server(load_balancer* main, unsigned int server_id) {
	// Check if the load balancer has virtual nodes
	if (main->enable_vnodes) {
		loader_remove_server_vnodes(main, server_id);
	} else {
		loader_remove_server_no_vnodes(main, server_id);
	}
}

response *loader_forward_request(load_balancer* main, request *req) {
	response *resp = NULL;

	// Find the document hash
	unsigned int doc_hash = main->hash_function_docs(req->doc_name);

	// Find the server that should handle the request
	for (unsigned int i = 0; i < main->servers_count; i++) {
		if (main->servers[i]->hash_ring_position > doc_hash) {
			resp = server_handle_request(main->servers[i], req);
			return resp;
		}
	}

	// If the document hash is greater than the last server's
	// hash_ring_position then the first server should handle the request
	resp = server_handle_request(main->servers[0], req);
	return resp;
}

void free_load_balancer(load_balancer** main) {
	if ((*main)->enable_vnodes) {
		for (unsigned int i = 0; i < (*main)->servers_count; i++) {
			if ((*main)->servers[i]->server_id < 100000) {
				free_server(&(*main)->servers[i]);
			} else {
				free_virtual_server(&(*main)->servers[i]);
			}
		}
		free(*main);

		*main = NULL;
	} else {
		for (unsigned int i = 0; i < (*main)->servers_count; i++) {
			free_server(&(*main)->servers[i]);
		}
		free(*main);

		*main = NULL;
	}
}

void sort_servers(load_balancer* main) {
	// Sort the servers by hash_ring_position
	while (true) {
		bool swapped = false;
		for (unsigned int i = 0; i < main->servers_count - 1; i++) {
			if (main->servers[i]->hash_ring_position >
				main->servers[i + 1]->hash_ring_position) {
				server *temp = main->servers[i];
				main->servers[i] = main->servers[i + 1];
				main->servers[i + 1] = temp;
				swapped = true;
			}
		}
		if (!swapped) {
			break;
		}
	}
}

void migrate_db_on_add(load_balancer* main, server* source_server,
						server* destination_server) {
	for (unsigned int i = 0; i < source_server->db->capacity; i++) {
		if (source_server->db->map[i]) {
			entry *entry = source_server->db->map[i];
			unsigned int doc_hash =
					main->hash_function_docs(
							source_server->db->map[i]->key);
			while (entry) {
				struct entry *next = entry->next_hash;

				// Find the documents that need to be migrated
				if (doc_hash < destination_server->hash_ring_position ||
					doc_hash > source_server->hash_ring_position) {
					db_put(destination_server->db,
						   source_server->db->map[i]->key,
						   source_server->db->map[i]->value);

					db_remove(source_server->db,
							  doc_hash % source_server->db->capacity,
							  source_server->db->map[i]->key);
				}
				entry = next;
			}
		}
	}
}

void migrate_cache_on_add(load_balancer* main, server* source_server,
						  server* destination_server) {
	// Cache variable
	lru_cache *cache = source_server->cache;

	for (unsigned int i = 0; i < cache->capacity; i++) {
		if (cache->map[i]) {
			entry *entry = cache->map[i];
			unsigned int doc_hash = main->hash_function_docs(
					cache->map[i]->key);
			while (entry) {
				struct entry *next = entry->next_hash;

				// Find the documents that need to be removed
				if (doc_hash < destination_server->hash_ring_position ||
					doc_hash > source_server->hash_ring_position) {
					lru_cache_remove(cache, doc_hash % cache->capacity,
									 cache->map[i]->key);
				}
				entry = next;
			}
		}
	}
}

void migrate_db_on_remove(load_balancer* main, server* source_server,
						  server* destination_server) {
	for (unsigned int i = 0; i < source_server->db->capacity; i++) {
		if (source_server->db->map[i]) {
			entry *entry = source_server->db->map[i];
			unsigned int doc_hash =
					main->hash_function_docs(
							source_server->db->map[i]->key);
			while (entry) {
				struct entry *next = entry->next_hash;
				// Migrate the documents
				db_put(destination_server->db,
					   source_server->db->map[i]->key,
					   source_server->db->map[i]->value);

				db_remove(source_server->db,
						  doc_hash % source_server->db->capacity,
						  source_server->db->map[i]->key);
				entry = next;
			}
		}
	}
}

void loader_add_server_no_vnodes(load_balancer* main, unsigned int server_id,
					   unsigned int cache_size) {
	// Create a new server
	main->servers[main->servers_count] = init_server(server_id, cache_size);
	main->servers_count++;

	// Migrate documents
	if (main->servers_count == 1) {
		return;
	} else {
		// Server variables
		server *source_server = NULL;
		server *destination_server = main->servers[main->servers_count - 1];

		// Find the source server
		for (unsigned int i = 0; i < main->servers_count; i++) {
			if (main->servers[i]->hash_ring_position >
				destination_server->hash_ring_position) {
				source_server = main->servers[i];
				break;
			}
		}

		// If the destination server is the last server in the ring
		if (!source_server) {
			source_server = main->servers[0];
		}

		// Execute all requests from the source server request queue
		server_execute_all_requests(source_server);

		// Migrate documents from the database
		migrate_db_on_add(main, source_server, destination_server);

		// Eliminate the documents from the cache
		// that are now in the destination server
		migrate_cache_on_add(main, source_server, destination_server);
	}

	// Sort the servers by hash_ring_position
	sort_servers(main);
}

void loader_add_server_vnodes(load_balancer* main, unsigned int server_id,
							  unsigned int cache_size) {
	// Create a new server
	main->servers[main->servers_count] = init_server(server_id, cache_size);

	// Create two virtual servers
	server *virtual_server1 = calloc(1, sizeof(server));
	server *virtual_server2 = calloc(1, sizeof(server));

	// Create the first virtual server
	virtual_server1->server_id = 1 * 100000 + server_id;
	virtual_server1->hash_ring_position = main->hash_function_servers(
			&virtual_server1->server_id);
	virtual_server1->request_queue =
			main->servers[main->servers_count]->request_queue;
	virtual_server1->cache = main->servers[main->servers_count]->cache;
	virtual_server1->db = main->servers[main->servers_count]->db;

	// Create the second virtual server
	virtual_server2->server_id = 2 * 100000 + server_id;
	virtual_server2->hash_ring_position = main->hash_function_servers(
			&virtual_server2->server_id);
	virtual_server2->request_queue =
			main->servers[main->servers_count]->request_queue;
	virtual_server2->cache = main->servers[main->servers_count]->cache;
	virtual_server2->db = main->servers[main->servers_count]->db;

	// Add the servers to the load balancer
	main->servers[main->servers_count + 1] = virtual_server1;
	main->servers[main->servers_count + 2] = virtual_server2;
	main->servers_count += 3;

	// Migrate documents
	if (main->servers_count == 3) {
		sort_servers(main);
		return;
	} else {
		for (unsigned int i = 0; i < 3; i++) {
			server *source_server = NULL;
			server *destination_server = main->servers[main->servers_count - i - 1];

			// Find the source server
			for (unsigned int j = 0; j < main->servers_count; j++) {
				if (main->servers[j]->hash_ring_position >
					destination_server->hash_ring_position) {
					source_server = main->servers[j];
					break;
				}
			}

			// If the destination server is the last server in the ring
			if (!source_server) {
				source_server = main->servers[0];
			}

			// Execute all requests from the source server request queue
			server_execute_all_requests(source_server);

			// Migrate documents from the database
			migrate_db_on_add(main, source_server, destination_server);

			// Eliminate the documents from the cache
			// that are now in the destination server
			migrate_cache_on_add(main, source_server, destination_server);
		}
	}

	sort_servers(main);
}

void loader_remove_server_no_vnodes(load_balancer* main,
									unsigned int server_id) {
	if (main->servers_count == 0) {
		return;
	} else if (main->servers_count == 1) {
		free_server(&main->servers[0]);
		main->servers_count--;
		return;
	} else {
		// Determine the source server
		server *destination_server = NULL;
		server *source_server = NULL;

		// Find the source server
		for (unsigned int i = 0; i < main->servers_count; i++) {
			if (main->servers[i]->server_id == server_id) {
				source_server = main->servers[i];
				break;
			}
		}

		// Find the destination server
		for (unsigned int i = 0; i < main->servers_count; i++) {
			if (main->servers[i]->hash_ring_position >
				source_server->hash_ring_position) {
				destination_server = main->servers[i];
				break;
			}
		}

		// If the source server is the last server in the ring
		if (!destination_server) {
			destination_server = main->servers[0];
		}

		// Execute all requests from the source server request queue
		server_execute_all_requests(source_server);

		// Migrate documents from the database
		// to the destination server's database
		migrate_db_on_remove(main, source_server, destination_server);
	}

	// Sort the servers by hash_ring_position and have the current
	// server_id be the last
	for (unsigned int i = 0; i < main->servers_count - 1; i++) {
		if (main->servers[i]->server_id == server_id) {
			server *temp = main->servers[i];
			main->servers[i] = main->servers[i + 1];
			main->servers[i + 1] = temp;
		}
	}

	// Free the server's memory
	free_server(&main->servers[main->servers_count - 1]);

	main->servers[main->servers_count - 1] = NULL;

	main->servers_count--;
}

void loader_remove_server_vnodes(load_balancer* main, unsigned int server_id) {
	if (main->servers_count == 0) {
		return;
	} else if (main->servers_count == 3) {
		free_server(&main->servers[0]);
		free_virtual_server(&main->servers[1]);
		free_virtual_server(&main->servers[2]);
		main->servers_count -= 3;
		return;
	} else {
		unsigned int id;
		for (unsigned int k = 0; k < 3; k++) {
			server *destination_server = NULL;
			server *source_server = NULL;
			for (unsigned int i = 0; i < main->servers_count - k; i++) {
				if (1 * 100000 + server_id == main->servers[i]->server_id ||
					2 * 100000 + server_id == main->servers[i]->server_id ||
					server_id == main->servers[i]->server_id) {
					id = main->servers[i]->server_id;
					source_server = main->servers[i];
					break;
				}
			}

			for (unsigned int j = 0; j < main->servers_count - k; j++) {
				if (main->servers[j]->hash_ring_position >
					source_server->hash_ring_position) {
					if (main->servers[j]->server_id != server_id &&
						main->servers[j]->server_id != 1 * 100000 + server_id &&
						main->servers[j]->server_id != 2 * 100000 + server_id) {
						destination_server = main->servers[j];
						break;
					}
				}
			}

			if (!destination_server) {
				destination_server = main->servers[0];
			}

			// Execute all requests from the source server request queue
			server_execute_all_requests(source_server);

			// Migrate documents from the database
			// to the destination server's database
			migrate_db_on_remove(main, source_server, destination_server);

			for (unsigned int j = 0; j < main->servers_count - 1 - k; j++) {
				if (id == main->servers[j]->server_id) {
					server *temp = main->servers[j];
					main->servers[j] = main->servers[j + 1];
					main->servers[j + 1] = temp;
				}
			}
		}
	}

	// Free the server's memory
	free_server(&main->servers[main->servers_count - 1]);
	free_virtual_server(&main->servers[main->servers_count - 2]);
	free_virtual_server(&main->servers[main->servers_count - 3]);

	main->servers[main->servers_count - 1] = NULL;
	main->servers[main->servers_count - 2] = NULL;
	main->servers[main->servers_count - 3] = NULL;

	main->servers_count -= 3;
}
