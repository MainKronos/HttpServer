#include "index.h"
#include <http_parser.h>
#include <http_server.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#define LOCAL_PATH "./resources/"

IMPORT_FILE(LOCAL_PATH "index.html", html_page);
IMPORT_FILE(LOCAL_PATH "style.css", css_style);
IMPORT_FILE(LOCAL_PATH "script.js", js_script);
IMPORT_FILE(LOCAL_PATH "favicon.ico", ico_favicon);

struct http_parser_transfer_ctx {
    const char *at;
    size_t length;
};

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

/* URL *********************************************************************************/



static int url_callback_on_url(http_parser* parser, const char *at, size_t length){
    struct http_parser_transfer_ctx* data = (struct http_parser_transfer_ctx*)parser->data;
    data->at = at;
    data->length = length;
    return 0;
}

int url_callback(int socket, void* data){
    http_parser parser; /* Istanza http parser */
    http_parser_settings settings; /* Istanza settings del parser */
    struct http_parser_transfer_ctx ctx; /* Informazioni di trasferimento */
    struct http_parser_url parser_url; /* Url parser */
    char buffer[HTTP_MAX_HEADER_SIZE]; /* Buffer per la richiesta */
    ssize_t recved; /* Bytes ricevuti */
    int ret;

    // Ricevo i dati
    if((recved = recv(socket, buffer, sizeof(buffer), 0)) <= 0) return -1;

    // inizializzo i setting del parser
    http_parser_settings_init(&settings);
    settings.on_url = url_callback_on_url;

    // inizializzo il parser
    http_parser_init(&parser, HTTP_REQUEST);
    // passo il buffer dei dati al parser
    parser.data = (void*)&ctx;

    ret = http_parser_execute(&parser, &settings, buffer, recved);
    if(HTTP_PARSER_ERRNO(&parser) != HPE_OK){
        fprintf(
            stderr, "%s: %s\r\n", 
            http_errno_name(HTTP_PARSER_ERRNO(&parser)),
            http_errno_description(HTTP_PARSER_ERRNO(&parser))
        );
        return -1;
    } 

    // inizializzo l'url parser
    http_parser_url_init(&parser_url);
    if(http_parser_parse_url(ctx.at, ctx.length, false, &parser_url) != 0) return -1;

    ret = snprintf(
        buffer + recved,
        sizeof(buffer) - recved,
        "PATH: %.*s\r\n"
        "QUERY: %.*s\r\n",
        parser_url.field_data[UF_PATH].len,
        ctx.at + parser_url.field_data[UF_PATH].off,
        parser_url.field_data[UF_QUERY].len,
        ctx.at + parser_url.field_data[UF_QUERY].off
    );

    return send_http_response(
        socket,
        HTTP_STATUS_OK,
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Connection: close\r\n",
        buffer + recved,
        ret
    );
}

/* AUTH ********************************************************************************/

static int auth_callback_on_header_field(http_parser* parser, const char* at, size_t length){
    struct http_parser_transfer_ctx* data = (struct http_parser_transfer_ctx*)parser->data;
    if(data->at != NULL) return 0;
    if(length != sizeof("Authorization") - 1) return 0;
    if(strncmp("Authorization", at, length) != 0) return 0;
    data->at = at;
    return 0;
}

static int auth_callback_on_header_value(http_parser* parser, const char* at, size_t length){
    struct http_parser_transfer_ctx* data = (struct http_parser_transfer_ctx*)parser->data;
    if(data->at == NULL) return 0;
    if(data->length != 0) return 0;
    if(length == 0) return 0;
    
    data->at = at;
    data->length = length;
    return 0;
}

int auth_callback(int socket, void* data){
    ssize_t ret;
    ssize_t recved;
    http_parser parser;
    http_parser_settings settings;
    struct http_parser_transfer_ctx ctx;
    char buffer[HTTP_MAX_HEADER_SIZE];
    const char pass[] = "Basic YWRtaW46YWRtaW4="; // admin admin

    // Segnalo che al compilatore che non uso la variabile data
    (void)data;

    // Ricevo i dati
    if((recved = recv(socket, buffer, sizeof(buffer), 0)) <= 0) return -1;

    http_parser_settings_init(&settings);
    settings.on_header_field = auth_callback_on_header_field;
    settings.on_header_value = auth_callback_on_header_value;

    http_parser_init(&parser, HTTP_REQUEST);
    memset(&ctx, 0, sizeof(ctx));
    parser.data = (void*)&ctx;

    http_parser_execute(&parser, &settings, buffer, recved);

    // Se ho trovato l'header di authorization
    if(ctx.at != NULL && ctx.length != 0){
        // Se la password corrisponde 
        if(strncmp(pass, ctx.at, ctx.length) == 0){
            strncpy(
                buffer,
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html; charset=utf-8\r\n"
                "Connection: close\r\n"
                "Content-Length: 7\r\n"
                "\r\n"
                "Welcome",
                sizeof(buffer)
            );
        }else{
            strncpy(
                buffer,
                "HTTP/1.1 401 Unauthorized\r\n"
                "Content-Type: text/html; charset=utf-8\r\n"
                "Connection: close\r\n"
                "Content-Length: 26\r\n"
                "\r\n"
                "WRONG USERNAME OR PASSWORD",
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
            "Content-Length: 12\r\n"
            "\r\n"
            "Unauthorized",
            sizeof(buffer)
        );
    }

    send(socket, buffer, strlen(buffer), 0);

    return 0;
}