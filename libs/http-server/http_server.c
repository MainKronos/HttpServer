#include "http_server.h"
#include <http_parser.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

/*** PRIVATE *******************************************************************/

/* Struttura di contesto del thread */
struct ThreadCtx {
    int socket; /* socket di invio/ricezione tcp */
    int index; /* indece handler che ha fatto match */
    const struct HttpHandler* handlers; /* array con tutti gli handlers */
};

/** Funzione del thread 
 * @param arg struct ThreadCtx
*/
static void* socket_handler(void* arg);

/** Funzione chiamata quando il parser trova un url
 * @param parser Istanza http_parser
 * @param at indirizzo inizio stringa url
 * @param length Lunghezza stringa url
 */
static int http_parser_on_url(http_parser* parser, const char *at, size_t length);

/******************************************************************************/

int http_server_init(struct HttpServer* this, const char address[], uint16_t port){
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
        memset(&this->_addr, 0, sizeof(this->_addr));
        this->_addr.sin_family = AF_INET;
        inet_pton(AF_INET, address ? address : "0.0.0.0", &this->_addr.sin_addr);
        this->_addr.sin_port = htons(port);

        // Aggancio del socket all'indirizzo
        if (bind(this->_listener, (struct sockaddr *)&this->_addr, sizeof(this->_addr)) == 0){
            // Creo la lista in entrata
            if (listen(this->_listener, 5) == 0){
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

    this->_running = false;
	return shutdown(this->_listener, SHUT_RDWR);
}

int http_server_run(struct HttpServer* this){
    struct ThreadCtx* ctx; /* contesto del thread */
	int ret; /* Valore di ritorno */
    struct sockaddr_in addr; /* Indirizzo client */
    pthread_t thread; /* Thread per la gestione della connessione */
    int socket; /* Socket che si è connesso al server */

    // Check iniziali
    if(this == NULL) return -1;
    if(this->_running) return -1;

    printf("%s:%d starting...\r\n", inet_ntoa(this->_addr.sin_addr), ntohs(this->_addr.sin_port));
    this->_running = true;

    /* --- Ciclo principale -------------------------------------------------------- */
    while(this->_running) {

		socket = accept(this->_listener, (struct sockaddr *)&addr, &(socklen_t){sizeof(addr)});
		if (socket < 0){
			perror("accept error");
			ret = -1;
			break;
		}

        ctx = (struct ThreadCtx*)malloc(sizeof(struct ThreadCtx));
		if(ctx == NULL){
			perror("malloc error");
            close(socket);
			ret = -1;
			break;
		}

        ctx->socket = socket;
        ctx->handlers = this->_handlers;
		printf("%s:%d connected\r\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

		/* Creazione thread per la gestione della connessione */
		ret = pthread_create(&thread, NULL, socket_handler, (void*)ctx);
	}

	/* --- Spegnimento Server ------------------------------------------------------------------- */
    
	printf("%s:%d closing...\r\n", inet_ntoa(this->_addr.sin_addr), ntohs(this->_addr.sin_port));
	fflush(stdout);
	close(this->_listener);
	return ret;
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
    struct http_parser_url parser_url; /* Url parser */
    int ret; /* Valore di ritorno */

    // inizializzo l'url parser
    http_parser_url_init(&parser_url);
    ret = http_parser_parse_url(at, length, false, &parser_url);
    if(ret != 0) return -1;
    
    // Stampo la richiesta ricevuto dal client
    getpeername(ctx->socket, (struct sockaddr *)&addr, &(socklen_t){sizeof(addr)});
    printf("%s:%d %s %.*s\r\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), http_method_str(parser->method), (int)length, at);

    // scorro tutti gli handler
    for(int i=0; i<HTTP_MAX_HANDLERS; i++){
        // se lo slot è valido
        if(ctx->handlers[i]._callback != NULL){
            ret = strncmp(ctx->handlers[i]._url, at + parser_url.field_data[UF_PATH].off, parser_url.field_data[UF_PATH].len);
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

	getpeername(ctx->socket, (struct sockaddr *)&addr, &(socklen_t){sizeof(addr)});

    // iniziallizzo i settings del parser
    http_parser_settings settings;
    http_parser_settings_init(&settings);
    settings.on_url = http_parser_on_url;

	while(1){
        // reset variabili 
        ctx->index = -2;
        recved = 0;

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
            ret = ctx->handlers[ctx->index]._callback(ctx->socket, ctx->handlers[ctx->index]._data);
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
