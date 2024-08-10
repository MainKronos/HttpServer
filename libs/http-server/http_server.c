#include "http_server.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

/*** PRIVATE *******************************************************************/

/* Struttura di contesto del thread */
struct ThreadCtx {
    int socket; /* socket di invio/ricezione tcp */
    int index; /* indece handler che ha fatto match */
    const struct HttpHandler* handlers; /* array con tutti gli handlers */
    struct HttpCallbackCtx* callback_ctx; /* Contesto callback da passare alla funzione di callback */
};

/** Funzione del thread 
 * @param arg struct ThreadCtx*
*/
static void* socket_handler(void* arg);

/** Funzione chiamata quando il parser trova un url
 * @param parser Istanza http_parser
 * @param at indirizzo inizio stringa url
 * @param length Lunghezza stringa url
 */
static int http_parser_on_url(http_parser* parser, const char *at, size_t length);


/** funzione del thread del server
 * @param arg struct HttpServer*
 * @return Se non ci sono stati errori ritorna 0
 */
static void* http_server_run(void* arg);

/** funzione di cleanup del thread del server
 * @param arg struct HttpServer*
 * @return Se non ci sono stati errori ritorna 0
 */
static void http_server_cleanup(void* arg);

/******************************************************************************/

int http_server_init(struct HttpServer* this, const char address[], uint16_t port){
    struct sockaddr_in addr; /* Indirizzo server */

    // Check iniziali
    if(this == NULL) return -1;
    if(port <= 0) return -1;

    // Pulisco la struttura
    memset(this, 0, sizeof(*this));

	// Creazione socket
	if((this->_listener = socket(AF_INET, SOCK_STREAM, 0)) >= 0){
        if(setsockopt(this->_listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))){
            perror("setsockopt error");
        }

        // Creazione indirizzo
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        inet_pton(AF_INET, address ? address : "0.0.0.0", &addr.sin_addr);
        addr.sin_port = htons(port);

        // Aggancio del socket all'indirizzo
        if (bind(this->_listener, (struct sockaddr *)&addr, sizeof(addr)) == 0){
            // Creo la lista in entrata
            if (listen(this->_listener, HTTP_MAX_WORKERS) == 0){
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
    if(!this->_running) return -1;

	if((pthread_cancel(this->_thread) ) == 0){
        return 0;
    }else{
        perror("pthread_cancel error");
        return -1;
    } 
}

static void http_server_cleanup(void* arg){
    struct HttpServer* this; /* HttpServer */
    struct sockaddr_in addr; /* Indirizzo server */

    this = (struct HttpServer*) arg;

    // Check iniziali
    if(this == NULL) return;

    getpeername(this->_listener, (struct sockaddr *)&addr, &(socklen_t){sizeof(addr)});
    printf("%s:%d closing...\r\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
    fflush(stdout);
    this->_running = false;
    close(this->_listener);
}

int http_server_start(struct HttpServer* this){
    // Check iniziali
    if(this == NULL) return -1;
    if(this->_running) return -1;

    if(pthread_create(&this->_thread, NULL, http_server_run, (void*)this) == 0){
        this->_running = true;
        return 0;
    }else perror("pthread_create error");
    return -1;
}

int http_server_join(struct HttpServer* this){
    int ret;
    void* tmp;

    // Check iniziali
    if(this == NULL) return -1;

    if(pthread_join(this->_thread, &tmp) != 0){
        perror("pthread_join error");
        return -1;
    }

    ret = *(int*)&tmp;
    return ret;
}

static void* http_server_run(void* arg){
    struct HttpServer* this; /* HttpServer */
    struct ThreadCtx* ctx; /* contesto del thread */
    struct sockaddr_in addr; /* Indirizzo client */
    pthread_t thread; /* Thread per la gestione della connessione */
    int socket; /* Socket che si è connesso al server */

    this = (struct HttpServer*)arg;

    pthread_cleanup_push(http_server_cleanup, this);

    getpeername(this->_listener, (struct sockaddr *)&addr, &(socklen_t){sizeof(addr)});
    printf("%s:%d starting...\r\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

    /* --- Ciclo principale -------------------------------------------------------- */
    while(true) {
        // Controllo se il thread è corretto
        if(pthread_equal(this->_thread, pthread_self())){
            if((socket = accept(this->_listener, (struct sockaddr *)&addr, &(socklen_t){sizeof(addr)})) >= 0 ){
                if((ctx = (struct ThreadCtx*)malloc(sizeof(struct ThreadCtx))) != NULL){
                    ctx->socket = socket;
                    ctx->handlers = this->_handlers;
                    printf("%s:%d connected\r\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

                    /* Creazione thread per la gestione della connessione */
                    if(pthread_create(&thread, NULL, socket_handler, (void*)ctx) == 0){
                        continue;
                    }else perror("pthread_create error");
                    free(ctx);
                }else perror("malloc error");
                close(socket);
            }else perror("accept error");
        }

    /* --- Spegnimento Server in caso di errore -------------------------------------------------------------- */
        pthread_exit((void*)-1);
        return NULL;
	}

    pthread_cleanup_pop(0);

	return NULL;
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
    struct ThreadCtx* ctx = (struct ThreadCtx*)parser->data; /* Contesto */
    struct sockaddr_in addr; /* Indirizzo client */
    struct http_parser_url* parser_url; /* Url parser */
    int ret; /* Valore di ritorno */

    parser_url = &ctx->callback_ctx->url;

    // inizializzo l'url parser
    http_parser_url_init(parser_url);
    ret = http_parser_parse_url(at, length, false, parser_url);
    if(ret != 0) return -1;
    
    // Stampo la richiesta ricevuto dal client
    getpeername(ctx->socket, (struct sockaddr *)&addr, &(socklen_t){sizeof(addr)});
    printf("%s:%d %s %.*s\r\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), http_method_str(parser->method), (int)length, at);

    // scorro tutti gli handler
    for(int i=0; i<HTTP_MAX_HANDLERS; i++){
        // se lo slot è valido
        if(ctx->handlers[i]._callback != NULL){
            ret = strncmp(ctx->handlers[i]._url, at + parser_url->field_data[UF_PATH].off, parser_url->field_data[UF_PATH].len);
            if(ret != 0) continue;
            ctx->index = i;
            return 0;
        }
    }
    // non ho trovato nulla
    ctx->index = -1; 
    return -1;
}

static void* socket_handler(void* arg) {
    struct ThreadCtx* ctx = ((struct ThreadCtx*)arg); /* contesto del thread */
	struct sockaddr_in addr; /* Indirizzo client */
    ssize_t recved; /* Bytes ricevuti */
    char request[HTTP_MAX_URL_SIZE]; /* Buffer per la richiesta */
    char response[512]; /* Buffer per la risposta (solo in caso di errore )*/
    int ret; /* Valore di ritorno */
    http_parser parser; /* Istanza http parser */
    struct HttpCallbackCtx callback_ctx; /* contesto callback */

	getpeername(ctx->socket, (struct sockaddr *)&addr, &(socklen_t){sizeof(addr)});

    // iniziallizzo i settings del parser
    http_parser_settings settings;
    http_parser_settings_init(&settings);
    settings.on_url = http_parser_on_url;

    

	while(1){
        // reset variabili 
        ctx->index = -2;
        recved = 0;

        // Collego il contesto della callback
        ctx->callback_ctx = &callback_ctx;
        memset(&callback_ctx, 0, sizeof(callback_ctx));

        // inizializzo il parser
        http_parser_init(&parser, HTTP_REQUEST);
        // passo il buffer dei dati al parser
        parser.data = (void*)ctx; 

        // ricevo i dati senza toglierli dallo stream
        recved = recv(ctx->socket, request, sizeof(request), MSG_PEEK);

        // se la connessione è stata chiusa o c'è un errore
        if (recved <= 0){
            if (recved < 0) perror("recv error");
            // Connessione chiusa
            break;
        }

        http_parser_execute(&parser, &settings, request, recved);

        // se è stato trovato un handler
        if(ctx->index >= 0){
            // Aggiorno il contesto della callback
            callback_ctx.socket = ctx->socket;
            callback_ctx.method = parser.method;
            callback_ctx.data = ctx->handlers[ctx->index]._data;
            ret = ctx->handlers[ctx->index]._callback(&callback_ctx);
            if(ret != 0){
                break; // chiudo la connessione
            }else{
                continue; // continuo alla prossima richiesta
            }
        }else // se non è stato trovato un handler
        if(ctx->index == -1){
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
        while((recved = recv(ctx->socket, request, sizeof(request), MSG_DONTWAIT) != -1 && errno != EWOULDBLOCK));

        // invio la risposta di errore
        send(ctx->socket, response, strlen(response), 0);
    }

    printf("%s:%d disconnected\r\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
    // chiudo il socket
    close(ctx->socket); 
    // libero la memoria
    free(arg); 
    // chiudo il thread
    pthread_exit(NULL); 
    return 0;
}
