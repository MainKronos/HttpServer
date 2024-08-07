# HttpServer
Micro http library written in c [UNIX]


## Example
```c
#include <http_server.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

int html_index_callback(int socket, void* data){
    char buffer[1024];

    // recv all request data
    while((recv(socket, buffer, sizeof(buffer), MSG_DONTWAIT) != -1 && errno != EWOULDBLOCK));

    // make response
    strncpy(
        buffer, 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: 5\r\n"
        "Connection: keep-alive\r\n"
        "\r\n"
        "INDEX",
        sizeof(buffer)
    );

    // send response
    send(socket, buffer, strlen(buffer), 0);

    // ret == 0 -> keep-alive,
    // ret != 0 -> close
    return 0;
}

int main(){
    struct HttpServer server;

    if(http_server_init(&server, NULL, 8080)) return -1;
    if(http_server_add_handler(&server, "/", html_index_callback, NULL)) return -1;
    if(http_server_run(&server)) return -1;
}
```