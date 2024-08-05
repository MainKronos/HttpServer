#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <http_server.h>
#include "index.h"

struct HttpServer server;

static void sigHandler(int signum);
static void sigHandler(int signum){
    if (signum == SIGINT) http_server_stop(&server);
}

int main(int argc, char* argv[]) {

	if (signal(SIGINT, sigHandler) == SIG_ERR)
        fprintf(stderr, "non posso intercettare SIGINT\r\n");
	
	if(http_server_init(&server, NULL, argc == 2 ? atoi(argv[1]) : 8080)) return -1;
	if(http_server_add_handler(&server, "/", html_index_callback, NULL)) return -1;
	if(http_server_add_handler(&server, "/styles.css", css_style_callback, NULL)) return -1;
	if(http_server_add_handler(&server, "/script.js", js_script_callback, NULL)) return -1;
	if(http_server_add_handler(&server, "/favicon.png", png_favicon_callback, NULL)) return -1;
	if(http_server_run(&server)) return -1;

	return 0;
}
