/*
 * MIT License
 * 
 * Copyright (c) 2024 Lorenzo Chesi
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _HTTP_SERVER_H_
#define _HTTP_SERVER_H_

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
    bool valid; /* se l'handler è valido */
    char url[1024]; /* url di match*/
    HttpCallback callback; /* funzione da chiamare in caso di match */
};

struct HttpServer {
    /* PRIVATE */
	struct sockaddr_in addr; /* Indirizzo server */
	int listener; /* Socket per l'ascolto */
    struct HttpHandler handlers[MAX_HANDLERS]; /* lista degli handler */
};

/** Inizializza la struttura Server */
int http_server_init(struct HttpServer* this, const char* address, uint16_t port);

/* Aggiunger un handler al server */
int http_server_add_handler(struct HttpServer* this, const char* url, HttpCallback callback);

/** Avvia il server */
int http_server_run(struct HttpServer* this);

#ifdef __cplusplus
}
#endif
#endif
