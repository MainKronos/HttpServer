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
IMPORT_FILE(LOCAL_PATH "file.bin", bin_file);

int js_script_callback(int socket){
    int ret = 0;
    char buffer[1024];

    // svuoto il buffer perchè non mi serve
    while((ret = recv(socket, buffer, sizeof(buffer), 0) == sizeof(buffer)));

    ret = snprintf(
        buffer, 
        sizeof(buffer), 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/javascript; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: keep-alive\r\n"
        "\r\n",
        _sizeof_js_script
    );

    if(ret < 0) return -1;

    send(socket, buffer, ret, 0);
    ret = 0;
    do{
        int tmp = send(socket, js_script + ret, _sizeof_js_script-ret, 0);
        if(tmp<0) return -1;
        ret += tmp;
    }while (ret != _sizeof_js_script);
    return 0;
}

int css_style_callback(int socket){
    int ret = 0;
    char buffer[1024];

    // svuoto il buffer perchè non mi serve
    while((ret = recv(socket, buffer, sizeof(buffer), 0) == sizeof(buffer)));

    ret = snprintf(
        buffer, 
        sizeof(buffer), 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/css; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: keep-alive\r\n"
        "\r\n",
        _sizeof_css_style
    );

    if(ret < 0) return -1;

    send(socket, buffer, ret, 0);
    ret = 0;
    do{
        int tmp = send(socket, css_style + ret, _sizeof_css_style-ret, 0);
        if(tmp<0) return -1;
        ret += tmp;
    }while (ret != _sizeof_css_style);
    return 0;
}

int html_index_callback(int socket){
    int ret = 0;
    char buffer[1024];

    // svuoto il buffer perchè non mi serve
    while((ret = recv(socket, buffer, sizeof(buffer), 0) == sizeof(buffer)));

    ret = snprintf(
        buffer, 
        sizeof(buffer), 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: keep-alive\r\n"
        "\r\n",
        _sizeof_html_page
    );

    if(ret < 0) return -1;

    send(socket, buffer, ret, 0);
    ret = 0;
    do{
        int tmp = send(socket, html_page + ret, _sizeof_html_page-ret, 0);
        if(tmp<0) return -1;
        ret += tmp;
    }while (ret != _sizeof_html_page);
    return 0;
}

int html_test_callback(int socket){
    int ret = 0;
    char buffer[1024];

    while((ret = recv(socket, buffer, sizeof(buffer), 0) == sizeof(buffer)));

    ret = snprintf(
        buffer, 
        sizeof(buffer), 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/octet-stream; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: keep-alive\r\n"
        "\r\n",
        _sizeof_bin_file
    );


    if(ret < 0) return -1;

    send(socket, buffer, ret, 0);

    ret = 0;
    do{
        int tmp = send(socket, bin_file+ret, _sizeof_bin_file-ret, 0);
        if(tmp<0) return -1;
        ret += tmp;
    }while (ret != _sizeof_bin_file);
    
    return 0;
}
