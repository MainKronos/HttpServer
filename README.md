# HttpServer
Micro http library written in c [UNIX]

## Utilizzo
1. Copiare i file `libs/http-server/http_server.c`, `libs/http-server/http_server.h`, `libs/http-parser/http_parser.c`, `libs/http-parser/http_parser.h` in una cartella ad esempio `libraries`.
2. Creare un file `main.c` dove includere le dipendenze (`#include <http_server.h>`)
3. Compilare usando il comando `gcc -Ilibraries libraries/http_server.c libraries/http_parser.c main.c`
4. Enjoy

## Limitazioni

- La connessione viene sempre chiusa dopo che è stato eseguito l'handler della relativa richiesta
- Se l'url della richiesta supera i 3000 caratteri allora potrebbe NON essere letto nella sua interezza

## Example

Alcuni esempi di callback si trovano nei file `index.h` e `index.c`.

```c
#include <http_server.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

int html_index_callback(int socket, void* data){
    // make response
    char buffer[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: 5\r\n"
        "Connection: close\r\n"
        "\r\n"
        "INDEX";

    // send response
    send(socket, buffer, sizeof(buffer) - 1, 0);

    return 0;
}

int main(){
    struct HttpServer server;

    if(http_server_init(&server, "127.0.0.1", 8080)) return -1;
    if(http_server_add_handler(&server, "/", html_index_callback, NULL)) return -1;
    if(http_server_start(&server)) return -1;

    /* other jobs */

    return http_server_join(&server);
}
```