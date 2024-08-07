#ifndef _INDEX_H_
#define _INDEX_H_

#include <stdlib.h>
#include <http_server.h>

int html_index_callback(int socket, void*);
int css_style_callback(int socket, void*);
int js_script_callback(int socket, void*);
int png_favicon_callback(int socket, void*);

int test1_callback(int socket, void*);
int test2_callback(int socket, void*);
int test3_callback(int socket, void*);
int test4_callback(int socket, void*);

#endif
