#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <http_server.h>
#include "index.h"

static struct HttpServer server;

static void sigHandler(int signum);
static void sigHandler(int signum){
    if (signum == SIGINT) http_server_stop(&server);
}

int main(int argc, char* argv[]) {

	if (signal(SIGINT, sigHandler) == SIG_ERR)
        fprintf(stderr, "non posso intercettare SIGINT\r\n");
	
	if(http_server_init(&server, NULL, argc == 2 ? atoi(argv[1]) : 8080)) return -1;
	if(http_server_add_handler(&server, HTTP_GET, "/", html_index_callback, NULL)) return -1;
	if(http_server_add_handler(&server, HTTP_GET, "/styles.css", css_style_callback, NULL)) return -1;
	if(http_server_add_handler(&server, HTTP_GET, "/script.js", js_script_callback, NULL)) return -1;
	if(http_server_add_handler(&server, HTTP_GET, "/favicon.ico", ico_favicon_callback, NULL)) return -1;

	if(http_server_add_handler(&server, HTTP_GET, "/stop", close_callback, &server)) return -1;
	if(http_server_add_handler(&server, HTTP_GET, "/test", html_test_callback, NULL)) return -1;
	if(http_server_add_handler(&server, HTTP_GET, "/lazy", lazy_image_callback, (void*)10)) return -1;
	if(http_server_add_handler(&server, HTTP_GET, "/auth", auth_callback, NULL)) return -1;

	if(http_server_add_handler(&server, HTTP_GET, "/test/0", test0_callback, NULL)) return -1;
	if(http_server_add_handler(&server, HTTP_GET, "/test/1", test1_callback, NULL)) return -1;
	if(http_server_add_handler(&server, HTTP_GET, "/test/2", test2_callback, NULL)) return -1;
	if(http_server_add_handler(&server, HTTP_GET, "/test/3", test3_callback, NULL)) return -1;
	if(http_server_add_handler(&server, HTTP_GET, "/test/4", test4_callback, NULL)) return -1;
	if(http_server_add_handler(&server, HTTP_GET, "/test/5", test5_callback, NULL)) return -1;
	if(http_server_add_handler(&server, HTTP_GET, "/test/6", test6_callback, NULL)) return -1;
	if(http_server_add_handler(&server, HTTP_GET, "/test/7", test7_callback, NULL)) return -1;

	if(http_server_start(&server)) return -1;
	return http_server_join(&server);
}
