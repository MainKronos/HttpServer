#ifndef _INDEX_H_
#define _INDEX_H_

#include <stdlib.h>
#include <http_server.h>

int html_index_callback(struct HttpCallbackCtx* ctx);
int css_style_callback(struct HttpCallbackCtx* ctx);
int js_script_callback(struct HttpCallbackCtx* ctx);
int png_favicon_callback(struct HttpCallbackCtx* ctx);

int close_callback(struct HttpCallbackCtx* ctx);

int test1_callback(struct HttpCallbackCtx* ctx);
int test2_callback(struct HttpCallbackCtx* ctx);
int test3_callback(struct HttpCallbackCtx* ctx);
int test4_callback(struct HttpCallbackCtx* ctx);

#endif
