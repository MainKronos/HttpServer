#ifndef _INDEX_H_
#define _INDEX_H_

#include <stdlib.h>
#include <http_server.h>

int html_index_callback(int socket);
int html_test_callback(int socket);
int css_style_callback(int socket);
int js_script_callback(int socket);

#endif
