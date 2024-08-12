#include "http_server.h"

#ifdef __linux__
#include <sys/select.h>
#else
#include <selectLib.h>
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

/*** PRIVATE *******************************************************************/

/* Struttura per il contesto della richiesta */
struct HttpRequestCtx {
    const struct HttpHandler* handlers; /* array con tutti gli handlers */
    HttpCallback callback; /* funzione da chiamare in caso di match */
    struct HttpCallbackCtx* callback_ctx; /* Contesto callback da passare alla funzione di callback */
};

/** Funzione del worker 
 * @param arg struct HttpWorkerCtx*
*/
static void* http_worker_run(void* arg);

/** Funzione chiamata quando il parser trova un url
 * @param parser Istanza http_parser
 * @param at indirizzo inizio stringa url
 * @param length Lunghezza stringa url
 */
static int http_parser_on_url(http_parser* parser, const char *at, size_t length);

/** funzione di cleanup del thread del workers
 * @param arg socket
 */
static void http_worker_cleanup(void* arg);

/******************************************************************************/

int http_server_init(struct HttpServer* this, const char address[], uint16_t port){
    // Check iniziali
    if(this == NULL) return -1;
    if(port <= 0) return -1;

    // Pulisco la struttura
    memset(this, 0, sizeof(*this));

	// Creazione socket
	if((this->_listener = socket(AF_INET, SOCK_STREAM, 0)) >= 0){
        if(setsockopt(this->_listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) != 0){
            perror("setsockopt error");
        }

        // Creazione indirizzo
        memset(&this->_addr, 0, sizeof(this->_addr));
        this->_addr.sin_family = AF_INET;
        inet_pton(AF_INET, address ? address : "0.0.0.0", &this->_addr.sin_addr);
        this->_addr.sin_port = htons(port);

        // Aggancio del socket all'indirizzo
        if (bind(this->_listener, (struct sockaddr *)&this->_addr, sizeof(this->_addr)) == 0){
            // Creo la lista in entrata
            if (listen(this->_listener, HTTP_MAX_WORKERS - 1) == 0){
                this->_state = HTTP_SERVER_INITIALIZED;
                return 0;
            } else perror("listen error");
        } else perror("bind error");
        close(this->_listener);
    } else perror("socket error");
    return -1;
}

int http_server_stop(struct HttpServer* this){
    // Check iniziali
    if(this == NULL) return -1;
    if(this->_state != HTTP_SERVER_RUNNING) return -1;
    if(this->_listener < 0) return -1;

    printf("%s:%d closing...\r\n", inet_ntoa(this->_addr.sin_addr), ntohs(this->_addr.sin_port));
    fflush(stdout);
    this->_state = HTTP_SERVER_STOPPED;
    for(int i=0; i<HTTP_MAX_WORKERS; i++){
        pthread_cancel(this->_workers[i]);
    }
    close(this->_listener);

    return 0;
}

int http_server_start(struct HttpServer* this){
    int i;

    // Check iniziali
    if(this == NULL) return -1;
    if(this->_state != HTTP_SERVER_INITIALIZED) return -1;

    printf("%s:%d starting...\r\n", inet_ntoa(this->_addr.sin_addr), ntohs(this->_addr.sin_port));
    
    for(i=0; i<HTTP_MAX_WORKERS; i++){
        if(pthread_create(&this->_workers[i], NULL, http_worker_run, (void*)this) != 0){
            perror("pthread_create error");
            break;
        }
    }
    // se tutti i worker sono stati creati
    if(i == HTTP_MAX_WORKERS){
        this->_state = HTTP_SERVER_RUNNING;
        return 0;
    }

    for(;i>=0; i--){
        pthread_cancel(this->_workers[i]);
    }

    close(this->_listener);
    memset(this, 0, sizeof(*this));

    return -1;
}

int http_server_join(struct HttpServer* this){
    void* tmp;

    // Check iniziali
    if(this == NULL) return -1;
    if(this->_state == HTTP_SERVER_INITIALIZED) return -1;

    for(int i=0; i<HTTP_MAX_WORKERS; i++){
        if(pthread_join(this->_workers[i], &tmp) != 0){
            perror("pthread_join error");
            return -1;
        }
    }

    return 0;
}

int http_server_add_handler(struct HttpServer* this, const char* url, HttpCallback callback, void* data){
    // Check iniziali
    if(this == NULL) return -1;
    if(url == NULL) return -1;
    if(callback == NULL) return -1;

    for(int i=0; i<HTTP_MAX_HANDLERS; i++){
        // se ho trovato uno slot libero
        if(this->_handlers[i]._callback == NULL){
            this->_handlers[i]._url = url;
            this->_handlers[i]._callback = callback;
            this->_handlers[i]._data = data;
            return 0;
        }
    }
    return -1;
}

static int http_parser_on_url(http_parser* parser, const char *at, size_t length){
    struct HttpRequestCtx* ctx = (struct HttpRequestCtx*)parser->data; /* Contesto */
    struct sockaddr_in addr; /* Indirizzo client */
    struct http_parser_url parser_url; /* Url parser */
    int ret; /* Valore di ritorno */

    // inizializzo l'url parser
    http_parser_url_init(&parser_url);
    ret = http_parser_parse_url(at, length, false, &parser_url);
    if(ret != 0) return -1;
    
    // Stampo la richiesta ricevuto dal client
    getpeername(ctx->callback_ctx->socket, (struct sockaddr *)&addr, &(socklen_t){sizeof(addr)});
    printf("%s:%d %s %.*s\r\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), http_method_str(parser->method), (int)length, at);

    // scorro tutti gli handler
    for(int i=0; i<HTTP_MAX_HANDLERS; i++){
        // se lo slot è valido
        if(ctx->handlers[i]._callback != NULL){
            ret = strncmp(ctx->handlers[i]._url, at + parser_url.field_data[UF_PATH].off, parser_url.field_data[UF_PATH].len);
            if(ret != 0) continue;
            // aggiorno il contesto della callback
            ctx->callback = ctx->handlers[i]._callback;
            ctx->callback_ctx->data = ctx->handlers[i]._data;
            ctx->callback_ctx->method = parser->method;
            return 0;
        }
    }
    // non ho trovato nulla
    return -1;
}


static void http_worker_cleanup(void* arg){
    int socket = *(int*)&arg; /* Socket richiesta */
    struct sockaddr_in addr; /* Indirizzo client */

    // Check iniziali
    if(socket < 0) return;

    getpeername(socket, (struct sockaddr *)&addr, &(socklen_t){sizeof(addr)});
    printf("%s:%d disconnected\r\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
    close(socket);
}

static void* http_worker_run(void* arg){
    struct HttpServer* this = (struct HttpServer*)arg; /* Contesto worker */
    struct sockaddr_in addr; /* Indirizzo client */
    ssize_t recved; /* Bytes ricevuti */
    char request[HTTP_MAX_URL_SIZE]; /* Buffer per la richiesta */
    char response[512]; /* Buffer per la risposta (solo in caso di errore )*/
    int ret; /* Valore di ritorno */
    http_parser parser; /* Istanza http parser */
    http_parser_settings settings; /* Istanza settings del parser */
    struct HttpRequestCtx request_ctx; /* Contesto richiesta */
    struct HttpCallbackCtx callback_ctx; /* contesto callback */
    int socket; /* Socket richiesta tcp */

    while(1){
        // Ottengo il socket della richiesta tcp
        if((socket = accept(this->_listener, (struct sockaddr *)&addr, &(socklen_t){sizeof(addr)})) < 0 ){
            perror("accept error");
            break;
        } 

        printf("%s:%d connected\r\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

        pthread_cleanup_push(http_worker_cleanup, *(void**)&socket);

        // inizializzo i setting del parser
        http_parser_settings_init(&settings);
        settings.on_url = http_parser_on_url;

        while(1){

            // Pulisco il contesto della richiesta
            memset(&request_ctx, 0, sizeof(request_ctx));
            request_ctx.handlers = this->_handlers;
            request_ctx.callback_ctx = &callback_ctx;
            request_ctx.callback = NULL;

            // Pulisco il contesto della callback
            memset(&callback_ctx, 0, sizeof(callback_ctx));
            callback_ctx.socket = socket;
            callback_ctx.server = this;

            // inizializzo il parser
            http_parser_init(&parser, HTTP_REQUEST);
            // passo il buffer dei dati al parser
            parser.data = (void*)&request_ctx;

            // ricevo i dati senza toglierli dallo stream
            recved = recv(socket, request, sizeof(request), MSG_PEEK);

            // se la connessione è stata chiusa o c'è un errore
            if (recved <= 0){
                if (recved < 0) perror("recv error");
                // Connessione chiusa
                break;
            }

            http_parser_execute(&parser, &settings, request, recved);

            // se è stato trovato un handler
            if(request_ctx.callback != NULL){
                int state;
                pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &state);
                ret = request_ctx.callback(&callback_ctx);
                pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &state);
                if(ret != 0){
                    break; // chiudo la connessione
                }else{
                    continue; // continuo alla prossima richiesta
                }
            }else // se non è stato trovato un handler
            if(request_ctx.callback == NULL){
                strncpy(response, 
                    "HTTP/1.1 404 Not Found\r\n"
                    "Content-Type: text/html; charset=utf-8\r\n"
                    "Content-Length: 0\r\n"
                    "Connection: keep-alive\r\n"
                    "\r\n",
                    sizeof(response)
                );
            }else // se c'è un errore
            if(HTTP_PARSER_ERRNO(&parser)){
                fprintf(
                    stderr, "%s:%d [%s] %s\r\n", 
                    inet_ntoa(addr.sin_addr), 
                    ntohs(addr.sin_port), 
                    http_errno_name(HTTP_PARSER_ERRNO(&parser)),
                    http_errno_description(HTTP_PARSER_ERRNO(&parser))
                );
                snprintf(
                    response, 
                    sizeof(response),
                    "HTTP/1.1 500 Internal Server Error\r\n"
                    "Content-Type: text/html; charset=utf-8\r\n"
                    "Content-Length: %ld\r\n"
                    "Connection: keep-alive\r\n"
                    "\r\n"
                    "[%s] %s",
                    strlen(http_errno_name(HTTP_PARSER_ERRNO(&parser))) + strlen(": ") + strlen(http_errno_description(HTTP_PARSER_ERRNO(&parser))),
                    http_errno_name(HTTP_PARSER_ERRNO(&parser)),
                    http_errno_description(HTTP_PARSER_ERRNO(&parser))
                );
            }

            // svuoto il buffer
            while((recved = recv(socket, request, sizeof(request), MSG_DONTWAIT) != -1 && errno != EWOULDBLOCK));

            // invio la risposta di errore
            send(socket, response, strlen(response), 0);
        }

        pthread_cleanup_pop(0);

        if(getpeername(socket, (struct sockaddr *)&addr, &(socklen_t){sizeof(addr)}) == 0){
            printf("%s:%d disconnected\r\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
        } else perror("getpeername error");
        
        // chiudo il socket
        close(socket); 
    }

    // chiudo il thread
    pthread_exit(NULL); 
    return NULL;
}

