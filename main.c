#include <stdlib.h>
#include <http_server.h>
#include "index.h"


int main(void) {
	struct Server server;

	if(server_init(&server, NULL, 8080)) return -1;
	if(server_add_handler(&server, &index_handler)) return -1;
	if(server_add_handler(&server, &test_handler)) return -1;
	if(server_run(&server)) return -1;

	
	return 0;
}
