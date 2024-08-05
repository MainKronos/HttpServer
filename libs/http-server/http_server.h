#ifndef _SERVER_H_
#define _SERVER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdbool.h>

#define MAX_HANDLERS 255

/**
 * Importa un file all'interno del codice.
 * @param file Percorso del file da includere
 * @param sym Simbolo da usare per creare il puntatore al file 
 * @return sym e _sizeof_sym che indicano lindirizzo di memoria e la grandezza del file
 */
#define IMPORT_FILE(file, sym) \
__asm__( \
    ".section \".rodata\" \n" \
    ".global " #sym "\n" \
    ".balign 16\n" \
    #sym":\n"  \
    ".incbin \"" file "\"\n" \
    \
    ".balign 16\n" \
    ".global _sizeof_" #sym "\n" \
    "_sizeof_" #sym":\n"  \
    ".long . - 1 - " #sym "\n" \
    ".section \".text\" \n" \
); \
extern __attribute__((aligned(16))) const size_t _sizeof_ ## sym; \
extern __attribute__((aligned(16))) const char sym[]

typedef int(*HttpCallback)(int socket);

struct HttpHandler {
    const char url[1024]; /* url di match*/
    HttpCallback callback; /* funzione da chiamare in caso di match */
};

struct HttpHandlerCtx {
    bool valid; /* se l'handler è valido */
    struct HttpHandler handler; /* handler */
};

struct HttpServer {
    /* PRIVATE */
	struct sockaddr_in addr; /* Indirizzo server */
	int listener; /* Socket per l'ascolto */
    int queue_len; /* Numero massimo di connessioni parallele */
    struct HttpHandlerCtx handler_list[MAX_HANDLERS]; /* lista degli handler */
};

/** Inizializza la struttura Server */
int http_server_init(struct HttpServer* this, const char* address, uint16_t port);

/* Aggiunger un handler al server */
int http_server_add_handler(struct HttpServer* this, const struct HttpHandler* handler);

/** Avvia il server */
int http_server_run(struct HttpServer* this);

#ifdef __cplusplus
}
#endif
#endif
