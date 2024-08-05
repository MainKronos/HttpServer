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
IMPORT_FILE(LOCAL_PATH "file.bin", bin_file);

const struct HttpHandler index_handler = {"/", html_index_callback};
const struct HttpHandler test_handler = {"/test/", html_test_callback};

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
