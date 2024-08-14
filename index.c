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
IMPORT_FILE(LOCAL_PATH "test.html", test_page);
IMPORT_FILE(LOCAL_PATH "style.css", css_style);
IMPORT_FILE(LOCAL_PATH "script.js", js_script);
IMPORT_FILE(LOCAL_PATH "favicon.ico", ico_favicon);

int close_callback(int socket, void* data){
    char buffer[1024];

    // fermo il server
    http_server_stop((struct HttpServer*)data);

    strncpy(
        buffer, 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Connection: close\r\n"
        "Content-Length: 13\r\n"
        "\r\n"
        "BYE BYE 👋",
        sizeof(buffer)
    );

    send(socket, buffer, strlen(buffer), 0);
    return 0;
}

int ico_favicon_callback(int socket, void* data){
    ssize_t ret = 0;
    char buffer[1024];

    // Segnalo che al compilatore che non uso la variabile data
    (void)data;

    ret = snprintf(
        buffer, 
        sizeof(buffer), 
        "HTTP/1.1 200 OK\r\n"
        "Connection: close\r\n"
        "Content-Type: image/ico\r\n"
        "Content-Length: %ld\r\n"
        "\r\n",
        _sizeof_ico_favicon
    );

    if(ret < 0) return -1;

    send(socket, buffer, ret, 0);
    ret = 0;
    do{
        int tmp = send(socket, ico_favicon + ret, _sizeof_ico_favicon-ret, 0);
        if(tmp<0) return -1;
        ret += tmp;
    }while ((size_t)ret != _sizeof_ico_favicon);
    return 0;
}

int js_script_callback(int socket, void* data){
    ssize_t ret = 0;
    char buffer[1024];

    // Segnalo che al compilatore che non uso la variabile data
    (void)data;

    ret = snprintf(
        buffer, 
        sizeof(buffer), 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/javascript; charset=utf-8\r\n"
        "Connection: close\r\n"
        "Content-Length: %ld\r\n"
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
    }while ((size_t)ret != _sizeof_js_script);
    return 0;
}

int css_style_callback(int socket, void* data){
    int ret = 0;
    char buffer[1024];

    // Segnalo che al compilatore che non uso la variabile data
    (void)data;

    ret = snprintf(
        buffer, 
        sizeof(buffer), 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/css; charset=utf-8\r\n"
        "Connection: close\r\n"
        "Content-Length: %ld\r\n"
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
    }while ((size_t)ret != _sizeof_css_style);
    return 0;
}

int html_index_callback(int socket, void* data){
    int ret = 0;
    char buffer[1024];

    // Segnalo che al compilatore che non uso la variabile data
    (void)data;

    ret = snprintf(
        buffer, 
        sizeof(buffer), 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Connection: close\r\n"
        "Content-Length: %ld\r\n"
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
    }while ((size_t)ret != _sizeof_html_page);
    return 0;
}

/*** TEST ******************************************************************************/

int test_callback(int socket, void* data){
    int ret = 0;
    char buffer[1024];

    // Segnalo che al compilatore che non uso la variabile data
    (void)data;

    ret = snprintf(
        buffer, 
        sizeof(buffer), 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Connection: close\r\n"
        "Content-Length: %ld\r\n"
        "\r\n",
        _sizeof_test_page
    );

    if(ret < 0) return -1;

    send(socket, buffer, ret, 0);
    ret = 0;
    do{
        int tmp = send(socket, test_page + ret, _sizeof_test_page-ret, 0);
        if(tmp<0) return -1;
        ret += tmp;
    }while ((size_t)ret != _sizeof_test_page);
    return 0;
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
        "Content-Length: 150\r\n"
        "Connection: close\r\n"
        "\r\n"
        "<!DOCTYPE html><html>"
        "<head><title>TEST 0</title></head>"
        "<body>Questo test usa una callback che preleva tutta i dati in arrivo dal socket.</body>"
        "</html>",
        sizeof(buffer)
    );

    send(socket, buffer, strlen(buffer), 0);
    return 0;
}

int test1_callback(int socket, void* data){
    // Segnalo che al compilatore che non uso la variabile data
    (void)data;

    char buffer[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: 146\r\n"
        "Connection: close\r\n"
        "\r\n"
        "<!DOCTYPE html><html>"
        "<head><title>TEST 1</title></head>"
        "<body>Questo test usa una callback che non preleva dati in arrivo dal socket.</body>"
        "</html>";

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
        "Content-Length: 167\r\n"
        "Connection: close\r\n"
        "\r\n"
        "<!DOCTYPE html><html>"
        "<head><title>TEST 2</title></head>"
        "<body>Questo test usa una callback che preleva parzialmente [10 byte] i dati in arrivo dal socket.</body>"
        "</html>",
        sizeof(buffer)
    );

    send(socket, buffer, strlen(buffer), 0);
    return 0;
}

int test3_callback(int socket, void* data){
    
    char buffer[] =  
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: 125\r\n"
        "Connection: close\r\n"
        "\r\n"
        "<!DOCTYPE html><html>"
        "<head><title>TEST 3</title></head>"
        "<body>Questo test usa una callback che chiude il socket.</body>"
        "</html>";
    
    // Segnalo che al compilatore che non uso la variabile data
    (void)data;

    send(socket, buffer, sizeof(buffer) - 1, 0);
    close(socket);

    return 0;
}

int test4_callback(int socket, void* data){
    char buffer[] =  
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: 200\r\n"
        "Connection: close\r\n"
        "\r\n"
        "<!DOCTYPE html><html>"
        "<head><title>TEST 4</title></head>"
        "<body>Questo test usa una callback che invia (con un unico pacchetto tcp) più byte rispetto a quelli dichiarati nel Content-Length.<br> 10 09 08 07 06 05 04 03 02 01 00</body>"
        "</html>";

    // Segnalo che al compilatore che non uso la variabile data
    (void)data;

    send(socket, buffer, sizeof(buffer) - 1, 0);

    return 0;
}

int test5_callback(int socket, void* data){
    char header[] =  
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: 217\r\n"
        "Connection: close\r\n"
        "\r\n";
    
    char body[] =  
        "<!DOCTYPE html><html>"
        "<head><title>TEST 5</title></head>"
        "<body>Questo test usa una callback che invia (con un 2 pacchetto tcp, header e body) più byte rispetto a quelli dichiarati nel Content-Length.<br> 10 09 08 07 06 05 04 03 02 01 00</body>"
        "</html>";

    // Segnalo che al compilatore che non uso la variabile data
    (void)data;

    send(socket, header, sizeof(header) - 1, 0);
    send(socket, body, sizeof(body) - 1, 0);

    return 0;
}

int test6_callback(int socket, void* data){
    char buffer[] =  
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: 250\r\n"
        "Connection: close\r\n"
        "\r\n"
        "<!DOCTYPE html><html>"
        "<head><title>TEST 6</title></head>"
        "<body>Questo test usa una callback che invia (con un unico pacchetto tcp) meno byte rispetto a quelli dichiarati nel Content-Length.</body>"
        "</html>";

    // Segnalo che al compilatore che non uso la variabile data
    (void)data;

    send(socket, buffer, sizeof(buffer) - 1, 0);

    return 0;
}

int test7_callback(int socket, void* data){
    char header[] =  
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: 250\r\n"
        "Connection: close\r\n"
        "\r\n";
    
    char body[] =  
        "<!DOCTYPE html><html>"
        "<head><title>TEST 7</title></head>"
        "<body>Questo test usa una callback che invia (con un 2 pacchetto tcp, header e body) meno byte rispetto a quelli dichiarati nel Content-Length.</body>"
        "</html>";

    // Segnalo che al compilatore che non uso la variabile data
    (void)data;

    send(socket, header, sizeof(header) - 1, 0);
    send(socket, body, sizeof(body) - 1, 0);

    return 0;
}

int test8_callback(int socket, void* data){
    int ret;
    char buffer[1024];

    // Genero un numero casuale tra [0s;10s)
    ret = rand() % 10;

    sleep(ret);

    ret = snprintf(
        buffer, sizeof(buffer), 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: 176\r\n"
        "Connection: close\r\n"
        "\r\n"
        "<!DOCTYPE html><html>"
        "<head><title>TEST 8</title></head>"
        "<body>Questo test usa una callback che invia la risposta dopo un tempo casuale [0s;10s); in questo caso %ds.</body>"
        "</html>", ret
    );        

    // Segnalo che al compilatore che non uso la variabile data
    (void)data;

    send(socket, buffer, ret, 0);

    return 0;
}