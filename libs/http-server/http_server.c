#include "http_server.h"
#include <http_parser.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

/* Struttura di contesto del thread */
struct ThreadCtx {
    int socket; /* socket di invio/ricezione tcp */
    int index; /* indece handler che ha fatto match */
    const struct HttpHandler* handlers; /* array con tutti gli handlers */
};

static void* socket_handler(void* arg);
static int on_url(http_parser* parser, const char *at, size_t length);

int http_server_init(struct HttpServer* this, const char* address, uint16_t port){
    int ret; /* Valore di ritorno */

    /* Check iniziali */
    if(this == NULL) return -1;
    if(port <= 0) return -1;

    /* Pulisco la struttura */
    memset(this, 0, sizeof(*this));

	/* Creazione socket */
	this->_listener = socket(AF_INET, SOCK_STREAM, 0);
    if(setsockopt(this->_listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))){
        perror("setsockopt error");
    }

	/* Creazione indirizzo */
	memset(&this->_addr, 0, sizeof(this->_addr));
	this->_addr.sin_family = AF_INET;
	inet_pton(AF_INET, address ? address : "0.0.0.0", &this->_addr.sin_addr);
	this->_addr.sin_port = htons(port);

	/* Aggancio del socket all'indirizzo */
	ret = bind(this->_listener, (struct sockaddr *)&this->_addr, sizeof(this->_addr));
	if (ret < 0){
		perror("Errore in fase di bind");
		return -1;
	}

	ret = listen(this->_listener, 5);
	if (ret < 0){
		perror("Errore in fase di listen");
		return -1;
	}

    return 0;
}

int http_server_stop(struct HttpServer* this){
    /* Check iniziali */
    if(this == NULL) return -1;
    if(!this->running) return -1;

    this->running = false;
	return shutdown(this->_listener, SHUT_RDWR);
}

int http_server_run(struct HttpServer* this){
    struct ThreadCtx* ctx; /* contesto del thread */
	int ret; /* Valore di ritorno */
    struct sockaddr_in addr; /* Indirizzo client */
    pthread_t thread; /* Thread per la gestione della connessione */

    /* Check iniziali */
    if(this == NULL) return -1;

    printf("Avvio server %s:%d\r\n", inet_ntoa(this->_addr.sin_addr), ntohs(this->_addr.sin_port));
    this->running = true;

    /* --- Ciclo principale -------------------------------------------------------- */
    while(this->running) {

		ctx = (struct ThreadCtx*)malloc(sizeof(struct ThreadCtx));
		if(ctx == NULL){
			perror("Errore malloc");
			ret = -1;
			break;
		}

		ctx->socket = accept(this->_listener, (struct sockaddr *)&addr, &(socklen_t){sizeof(addr)});
		if (ctx->socket < 0){
			perror("Errore in fase di accept");
            free(ctx);
			ret = -1;
			break;
		}
        ctx->handlers = this->_handlers;
		printf("Nuova connessione da %s:%d\r\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

		/* Creazione thread per la gestione della connessione */
		ret = pthread_create(&thread, NULL, socket_handler, (void*)ctx);
	}

	/* --- Spegnimento Server ------------------------------------------------------------------- */
    
	printf("Spegnimento server...\r\n");
	fflush(stdout);
	close(this->_listener);
	return ret;
}

int http_server_add_handler(struct HttpServer* this, const char* url, HttpCallback callback, void* data){
    /* Check iniziali */
    if(this == NULL) return -1;

    for(int i=0; i<HTTP_MAX_HANDLERS; i++){
        // se ho trovato uno slot libero
        if(!this->_handlers[i]._valid){
            this->_handlers[i]._valid = true;
            strncpy(this->_handlers[i]._url, url, sizeof(this->_handlers[i]._url));
            this->_handlers[i]._callback = callback;
            this->_handlers[i]._data = data;
            return 0;
        }
    }
    return -1;
}

static int on_url(http_parser* parser, const char *at, size_t length){
    struct sockaddr_in addr; /* Indirizzo client */
    struct ThreadCtx* ctx = (struct ThreadCtx*)parser->data;
    struct http_parser_url parser_url;
    int ret;

    // inizializzo l'url parser
    http_parser_url_init(&parser_url);
    ret = http_parser_parse_url(at, length, false, &parser_url);
    if(ret != 0) return -1;

    getpeername(ctx->socket, (struct sockaddr *)&addr, &(socklen_t){sizeof(addr)});
    printf("%s:%d %s %.*s\r\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), http_method_str(parser->method), (int)length, at);

    /* scorro tutti gli handler */
    for(int i=0; i<HTTP_MAX_HANDLERS; i++){
        /* se lo slot è valido */
        if(ctx->handlers[i]._valid){
            ret = strncmp(ctx->handlers[i]._url, at + parser_url.field_data[UF_PATH].off, parser_url.field_data[UF_PATH].len);
            if(ret != 0) continue;
            ctx->index = i;
            return 0;
        }
    }
    return -1;
}

static void* socket_handler(void* arg) {
    struct ThreadCtx* ctx = ((struct ThreadCtx*)arg); /* contesto del thread */
	struct sockaddr_in addr; /* Indirizzo client */
    ssize_t recved;
    char request[1024];
    char response[1024];
    ssize_t response_size;
    int ret;

	getpeername(ctx->socket, (struct sockaddr *)&addr, &(socklen_t){sizeof(addr)});

    http_parser parser;

    // iniziallizzo i settings del parser
    http_parser_settings settings;
    http_parser_settings_init(&settings);
    settings.on_url = on_url;

	while(1){
        /* setup */
        ctx->index = -1;
        response_size = sizeof(response);
        recved = 0;

        // inizializzo il parser
        http_parser_init(&parser, HTTP_REQUEST);
        parser.data = (void*)ctx; // passo il buffer dei dati al parser

        // ricevo i dati senza toglierli dallo stream
        recved = recv(ctx->socket, request, sizeof(request), MSG_PEEK);

        // se la connessione è stata chiusa o c'è un errore
        if (recved <= 0){
            if (recved < 0) perror("Errore in fase di ricezione");
            /* Connessione chiusa */
            printf("Connessione chiusa da %s:%d\r\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
            break;
        }

        http_parser_execute(&parser, &settings, request, recved);

        // se è stato trovato un handler
        if(ctx->index != -1){
            ret = ctx->handlers[ctx->index]._callback(ctx->socket, ctx->handlers[ctx->index]._data);
            if(ret != 0){
                break; // chiudo la connessione
            }else{
                continue; // continuo alla prossima richiesta
            }
        }else /* se non è stato trovato un handler */
        if(HTTP_PARSER_ERRNO(&parser) == HPE_CB_url || ctx->index == -1){
            strncpy(response, 
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Type: text/html; charset=utf-8\r\n"
                "Content-Length: 0\r\n"
                "Connection: keep-alive\r\n"
                "\r\n",
                sizeof(response)
            );
            response_size = strlen(response);
        }else /* se c'è un errore */
        if(HTTP_PARSER_ERRNO(&parser)){
            snprintf(
                response, 
                sizeof(response),
                "HTTP/1.1 500 Internal Server Error\r\n"
                "Content-Type: text/html; charset=utf-8\r\n"
                "Content-Length: %ld\r\n"
                "Connection: keep-alive\r\n"
                "\r\n"
                "%s: %s",
                strlen(http_errno_name(HTTP_PARSER_ERRNO(&parser))) + strlen(": ") + strlen(http_errno_description(HTTP_PARSER_ERRNO(&parser))),
                http_errno_name(HTTP_PARSER_ERRNO(&parser)),
                http_errno_description(HTTP_PARSER_ERRNO(&parser))
            );
            response_size = strlen(response);
        }

        // svuoto il buffer
        while((recved = recv(ctx->socket, request, sizeof(request), MSG_DONTWAIT) != -1 && errno != EWOULDBLOCK));

        // invio la risposta di errore
        send(ctx->socket, response, response_size, 0);
    }

    close(ctx->socket); // chiudo il socket
    free(arg); // libero la memoria
    pthread_exit(NULL); // chiudo il thread
    return 0;
}
