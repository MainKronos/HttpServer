#include "index.h"
#include <http_parser.h>
#include <http_server.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#define LOCAL_PATH "./resources/"

IMPORT_FILE(LOCAL_PATH "index.html", html_page);
IMPORT_FILE(LOCAL_PATH "style.css", css_style);
IMPORT_FILE(LOCAL_PATH "script.js", js_script);
IMPORT_FILE(LOCAL_PATH "favicon.png", png_favicon);

int close_callback(struct HttpCallbackCtx* ctx){
    ssize_t ret = 0;
    char buffer[1024];

    // svuoto il buffer perchè non mi serve
    while((ret = recv(ctx->socket, buffer, sizeof(buffer), MSG_DONTWAIT) != -1 && errno != EWOULDBLOCK));

    strncpy(
        buffer, 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: 13\r\n"
        "Connection: keep-alive\r\n"
        "\r\n"
        "BYE BYE 👋",
        sizeof(buffer)
    );

    send(ctx->socket, buffer, strlen(buffer), 0);
    http_server_stop(ctx->server);
    return 0;
}

int png_favicon_callback(struct HttpCallbackCtx* ctx){
    ssize_t ret = 0;
    char buffer[1024];

    // svuoto il buffer perchè non mi serve
    while((ret = recv(ctx->socket, buffer, sizeof(buffer), MSG_DONTWAIT) != -1 && errno != EWOULDBLOCK));

    ret = snprintf(
        buffer, 
        sizeof(buffer), 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: image/png\r\n"
        "Content-Length: %ld\r\n"
        "Connection: keep-alive\r\n"
        "\r\n",
        _sizeof_png_favicon
    );

    if(ret < 0) return -1;

    send(ctx->socket, buffer, ret, 0);
    ret = 0;
    do{
        int tmp = send(ctx->socket, png_favicon + ret, _sizeof_png_favicon-ret, 0);
        if(tmp<0) return -1;
        ret += tmp;
    }while ((size_t)ret != _sizeof_png_favicon);
    return 0;
}

int js_script_callback(struct HttpCallbackCtx* ctx){
    ssize_t ret = 0;
    char buffer[1024];

    // svuoto il buffer perchè non mi serve
    while((ret = recv(ctx->socket, buffer, sizeof(buffer), MSG_DONTWAIT) != -1 && errno != EWOULDBLOCK));

    ret = snprintf(
        buffer, 
        sizeof(buffer), 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/javascript; charset=utf-8\r\n"
        "Content-Length: %ld\r\n"
        "Connection: keep-alive\r\n"
        "\r\n",
        _sizeof_js_script
    );

    if(ret < 0) return -1;

    send(ctx->socket, buffer, ret, 0);
    ret = 0;
    do{
        int tmp = send(ctx->socket, js_script + ret, _sizeof_js_script-ret, 0);
        if(tmp<0) return -1;
        ret += tmp;
    }while ((size_t)ret != _sizeof_js_script);
    return 0;
}

int css_style_callback(struct HttpCallbackCtx* ctx){
    int ret = 0;
    char buffer[1024];

    // svuoto il buffer perchè non mi serve
    while((ret = recv(ctx->socket, buffer, sizeof(buffer), MSG_DONTWAIT) != -1 && errno != EWOULDBLOCK));

    ret = snprintf(
        buffer, 
        sizeof(buffer), 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/css; charset=utf-8\r\n"
        "Content-Length: %ld\r\n"
        "Connection: keep-alive\r\n"
        "\r\n",
        _sizeof_css_style
    );

    if(ret < 0) return -1;

    send(ctx->socket, buffer, ret, 0);
    ret = 0;
    do{
        int tmp = send(ctx->socket, css_style + ret, _sizeof_css_style-ret, 0);
        if(tmp<0) return -1;
        ret += tmp;
    }while ((size_t)ret != _sizeof_css_style);
    return 0;
}

int html_index_callback(struct HttpCallbackCtx* ctx){
    int ret = 0;
    char buffer[1024];

    // svuoto il buffer perchè non mi serve
    while((ret = recv(ctx->socket, buffer, sizeof(buffer), MSG_DONTWAIT) != -1 && errno != EWOULDBLOCK));

    ret = snprintf(
        buffer, 
        sizeof(buffer), 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %ld\r\n"
        "Connection: keep-alive\r\n"
        "\r\n",
        _sizeof_html_page
    );

    if(ret < 0) return -1;

    send(ctx->socket, buffer, ret, 0);
    ret = 0;
    do{
        int tmp = send(ctx->socket, html_page + ret, _sizeof_html_page-ret, 0);
        if(tmp<0) return -1;
        ret += tmp;
    }while ((size_t)ret != _sizeof_html_page);
    return 0;
}
