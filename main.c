#include <stdlib.h>
#include <http_server.h>
#include "index.h"


int main(void) {
	struct HttpServer server;

	if(http_server_init(&server, NULL, 8080)) return -1;
	if(http_server_add_handler(&server, "/", html_index_callback, NULL)) return -1;
	if(http_server_add_handler(&server, "/test/", html_test_callback, NULL)) return -1;
	if(http_server_add_handler(&server, "/styles.css", css_style_callback, NULL)) return -1;
	if(http_server_add_handler(&server, "/script.js", js_script_callback, NULL)) return -1;
	if(http_server_run(&server)) return -1;

	return 0;
}
