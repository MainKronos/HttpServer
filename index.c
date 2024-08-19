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
IMPORT_FILE(LOCAL_PATH "test.html", html_test_page);
IMPORT_FILE(LOCAL_PATH "style.css", css_style);
IMPORT_FILE(LOCAL_PATH "script.js", js_script);
IMPORT_FILE(LOCAL_PATH "favicon.ico", ico_favicon);

int close_callback(int socket, void* data){
    char buffer[] = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Connection: close\r\n"
        "Content-Length: 13\r\n"
        "\r\n"
        "BYE BYE 👋";

    // fermo il server
    http_server_stop((struct HttpServer*)data);

    send(socket, buffer, sizeof(buffer) - 1, 0);
    return 0;
}

int ico_favicon_callback(int socket, void* data){
    // Segnalo al compilatore che non uso la variabile data
    (void)data;

    return send_http_response(
        socket, 
        HTTP_STATUS_OK, 
        "Connection: close\r\n"
        "Content-Type: image/ico\r\n",
        ico_favicon,
        _sizeof_ico_favicon
    );
}

int js_script_callback(int socket, void* data){
    // Segnalo al compilatore che non uso la variabile data
    (void)data;

    return send_http_response(
        socket, 
        HTTP_STATUS_OK,
        "Connection: close\r\n"
        "Content-Type: text/javascript; charset=utf-8\r\n",
        js_script,
        _sizeof_js_script
    );
}

int css_style_callback(int socket, void* data){
    // Segnalo che al compilatore che non uso la variabile data
    (void)data;

    return send_http_response(
        socket, 
        HTTP_STATUS_OK,
        "Connection: close\r\n"
        "Content-Type: text/css; charset=utf-8\r\n",
        css_style,
        _sizeof_css_style
    );
}

int html_index_callback(int socket, void* data){
    // Segnalo che al compilatore che non uso la variabile data
    (void)data;

    return send_http_response(
        socket, 
        HTTP_STATUS_OK,
        "Content-Type: text/html; charset=utf-8\r\n"
        "Connection: close\r\n",
        html_page,
        _sizeof_html_page
    );
}

int lazy_image_callback(int socket, void* data){
    int ret;
    char buffer[1024];

    ret = rand() % (*(int*)&data);

    sleep(ret);

    ret = snprintf(
        buffer, sizeof(buffer), 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: image/svg+xml\r\n"
        "Content-Length: 276\r\n"
        "Connection: close\r\n"
        "\r\n"
        "<svg width='100' height='100' version='1.1' xmlns='http://www.w3.org/2000/svg'>"
        "<circle cx='50' cy='50' r='40' stroke='black' stroke-width='5' fill='none'/>"
        "<text x='50%%' y='50%%' style='font: bold 50px monospace;' dominant-baseline='central' text-anchor='middle'>%02d</text>"
        "</svg>", ret
    );

    send(socket, buffer, ret, 0);

    return 0;
}

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

/* AUTH ********************************************************************************/

struct auth_callback_data {
    bool authorization_header;
    const char* authorization;
    size_t authorization_length;
};


static int auth_callback_on_header_field(http_parser* parser, const char* at, size_t length){
    struct auth_callback_data* data = (struct auth_callback_data*)parser->data;
    const char authorization_header[] = "Authorization";
    if(data->authorization_header) return 0;
    if(length != sizeof(authorization_header) - 1) return 0;
    if(strncmp(authorization_header, at, length) != 0) return 0;
    data->authorization_header = true;
    return 0;
}

static int auth_callback_on_header_value(http_parser* parser, const char* at, size_t length){
    struct auth_callback_data* data = (struct auth_callback_data*)parser->data;
    if(!data->authorization_header) return 0;
    if(data->authorization != NULL) return 0;
    data->authorization = at;
    data->authorization_length = length;
    return 0;
}

int auth_callback(int socket, void* data){
    ssize_t ret;
    ssize_t size;
    http_parser parser;
    http_parser_settings settings;
    struct auth_callback_data auth_data;
    char buffer[HTTP_MAX_HEADER_SIZE];
    const char pass[] = "Basic YWRtaW46YWRtaW4="; // admin admin

    // Segnalo che al compilatore che non uso la variabile data
    (void)data;

    // ricevo il messaggio
    size = 0;
    while(1){
        ret = recv(socket, buffer + size, sizeof(buffer) - size, MSG_DONTWAIT);
        if(ret == -1) break;
        size += ret;
        if((size_t)size >= sizeof(buffer) || errno != EWOULDBLOCK) break;
    }

    http_parser_settings_init(&settings);
    settings.on_header_field = auth_callback_on_header_field;
    settings.on_header_value = auth_callback_on_header_value;

    http_parser_init(&parser, HTTP_REQUEST);
    memset(&auth_data, 0, sizeof(auth_data));
    parser.data = (void*)&auth_data;

    http_parser_execute(&parser, &settings, buffer, size);

    // Se ho trovato l'header di authorization
    if(auth_data.authorization_header && auth_data.authorization != NULL){
        // Se la password corrisponde 
        if(strncmp(pass, auth_data.authorization, auth_data.authorization_length) == 0){
            strncpy(
                buffer,
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html; charset=utf-8\r\n"
                "Connection: close\r\n"
                "Content-Length: 2\r\n"
                "\r\n"
                "OK",
                sizeof(buffer)
            );
        }else{
            strncpy(
                buffer,
                "HTTP/1.1 401 Unauthorized\r\n"
                "Content-Type: text/html; charset=utf-8\r\n"
                "Connection: close\r\n"
                "Content-Length: 12\r\n"
                "\r\n"
                "Unauthorized",
                sizeof(buffer)
            );
        }
    }else{
        strncpy(
            buffer,
            "HTTP/1.1 401 Unauthorized\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "WWW-Authenticate: Basic realm=\"User Visible Realm\"\r\n"
            "Connection: close\r\n"
            "Content-Length: 0\r\n"
            "\r\n",
            sizeof(buffer)
        );
    }

    send(socket, buffer, strlen(buffer), 0);

    return 0;
}

/* TEST ******************************************************************************/

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
        "Content-Description: Questo test usa una callback che preleva tutta i dati in arrivo dal socket\r\n"
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
        "Content-Description: Questo test usa una callback che non preleva dati in arrivo dal socket\r\n"
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
        "Content-Description: Questo test usa una callback che preleva parzialmente [10 byte] i dati in arrivo dal socket\r\n"
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
        "Content-Description: Questo test usa una callback che chiude il socket\r\n"
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
        "Content-Description: Questo test usa una callback che invia (con un unico pacchetto tcp) più byte rispetto a quelli dichiarati nel Content-Length\r\n"
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
        "Content-Description: Questo test usa una callback che invia (con un 2 pacchetto tcp, header e body) più byte rispetto a quelli dichiarati nel Content-Length\r\n"
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
        "Content-Description: Questo test usa una callback che invia (con un unico pacchetto tcp) meno byte rispetto a quelli dichiarati nel Content-Length\r\n"
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
        "Content-Description: Questo test usa una callback che invia (con un 2 pacchetto tcp, header e body) meno byte rispetto a quelli dichiarati nel Content-Length\r\n"
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