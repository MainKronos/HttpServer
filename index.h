#ifndef _INDEX_H_
#define _INDEX_H_

#include <stdlib.h>
#include <http_server.h>

int html_index_callback(struct HttpCallbackCtx* ctx);
int css_style_callback(struct HttpCallbackCtx* ctx);
int js_script_callback(struct HttpCallbackCtx* ctx);
int png_favicon_callback(struct HttpCallbackCtx* ctx);

#endif
