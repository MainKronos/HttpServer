#ifndef _INDEX_H_
#define _INDEX_H_

#include <stdlib.h>
#include <http_server.h>

int html_index_callback(int socket);
int html_test_callback(int socket);

extern const struct HttpHandler index_handler;
extern const struct HttpHandler test_handler;

#endif
