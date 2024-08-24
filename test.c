#include "test.h"

#include <http_parser.h>
#include <http_server.h>

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

IMPORT_FILE("./resources/test.html", html_test_page);

int html_test_callback(int socket, void* data){
    // Segnalo che al compilatore che non uso la variabile data
    (void)data;

    return send_http_response(
        socket, 
        HTTP_STATUS_OK,
        "Content-Type: text/html; charset=utf-8\r\n"
        "Connection: close\r\n",
        html_test_page,
        _sizeof_html_test_page
    );
}

int test0_callback(int socket, void* data){
    ssize_t ret;
    char buffer[1024];

    // Segnalo che al compilatore che non uso la variabile data
    (void)data;

    // svuoto il buffer perchè non mi serve
    while((ret = recv(socket, buffer, sizeof(buffer), MSG_DONTWAIT) != -1 && errno != EWOULDBLOCK));

    strncpy(
        buffer, 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n",
        sizeof(buffer)
    );

    send(socket, buffer, strlen(buffer), 0);
    return 0;
}

int test1_callback(int socket, void* data){
    // Segnalo che al compilatore che non uso la variabile data
    (void)data;

    const char buffer[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n";

    send(socket, buffer, sizeof(buffer) - 1, 0);
    return 0;
}

int test2_callback(int socket, void* data){
    char buffer[1024];

    // Segnalo che al compilatore che non uso la variabile data
    (void)data;

    recv(socket, buffer, 10, 0);

    strncpy(
        buffer,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n",
        sizeof(buffer)
    );

    send(socket, buffer, strlen(buffer), 0);
    return 0;
}

int test3_callback(int socket, void* data){
    
    const char buffer[] =  
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n";
    
    // Segnalo che al compilatore che non uso la variabile data
    (void)data;

    send(socket, buffer, sizeof(buffer) - 1, 0);
    close(socket);

    return 0;
}

int test4_callback(int socket, void* data){
    const char buffer[] =  
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: 20\r\n"
        "Connection: close\r\n"
        "\r\n"
        "10 09 08 07 06 05 04 03 02 01 00";

    // Segnalo che al compilatore che non uso la variabile data
    (void)data;

    send(socket, buffer, sizeof(buffer) - 1, 0);

    return 0;
}

int test5_callback(int socket, void* data){
    const char header[] =  
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: 20\r\n"
        "Connection: close\r\n"
        "\r\n";
    
    const char body[] =  
        "10 09 08 07 06 05 04 03 02 01 00";

    // Segnalo che al compilatore che non uso la variabile data
    (void)data;

    send(socket, header, sizeof(header) - 1, 0);
    send(socket, body, sizeof(body) - 1, 0);

    return 0;
}

int test6_callback(int socket, void* data){
    const char buffer[] =  
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: 50\r\n"
        "Connection: close\r\n"
        "\r\n"
        "10 09 08 07 06 05 04 03 02 01 00";

    // Segnalo che al compilatore che non uso la variabile data
    (void)data;

    send(socket, buffer, sizeof(buffer) - 1, 0);

    return 0;
}

int test7_callback(int socket, void* data){
    const char header[] =  
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: 50\r\n"
        "Content-Description: \r\n"
        "Connection: close\r\n"
        "\r\n";
    
    const char body[] =  
        "10 09 08 07 06 05 04 03 02 01 00";

    // Segnalo che al compilatore che non uso la variabile data
    (void)data;

    send(socket, header, sizeof(header) - 1, 0);
    send(socket, body, sizeof(body) - 1, 0);

    return 0;
}