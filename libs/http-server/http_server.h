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

#ifndef HTTP_MAX_URL_SIZE
/** Dimenzione massima dell'url */
#define HTTP_MAX_URL_SIZE 256
#endif

#ifndef HTTP_MAX_HANDLERS
/** Numero massimo di handlers */
#define HTTP_MAX_HANDLERS 128
#endif

#ifndef HTTP_MAX_THREADS
/** Numero massimo di threads */
#define HTTP_MAX_THREADS 10
#endif

/** Importa un file all'interno del codice.
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
    #sym "_end:\n" \
    ".balign 16\n" \
    ".global _sizeof_" #sym "\n" \
    "_sizeof_" #sym":\n"  \
    ".long " #sym "_end - " #sym "\n" \
    ".section \".text\" \n" \
); \
extern __attribute__((aligned(16))) const size_t _sizeof_ ## sym; \
extern __attribute__((aligned(16))) const char sym[]

/** Funzione di callback
 * @param socket Socket per la ricezione e l'invio dei dati
 * @param data puntatore a memoria dati definita dall'utente
 * @return Ritorna 0 per keep-alive altrimenti chiude la connessione
 */
typedef int(*HttpCallback)(int socket, void* data);

struct HttpHandler {
    /* PRIVATE */
    HttpCallback _callback; /* funzione da chiamare in caso di match */
    char _url[HTTP_MAX_URL_SIZE]; /* url di match*/
    void* _data; /* puntatore a memoria dati definita dall'utente */
};

struct HttpServer {
    /* PRIVATE */
    int _listener; /* Socket per l'ascolto */
    bool _running; /* Indica che il server è in esecuzione */
	struct sockaddr_in _addr; /* Indirizzo server */
    struct HttpHandler _handlers[HTTP_MAX_HANDLERS]; /* lista degli handler */
};

/** Inizializza la struttura Server 
 * @param this Istanza dell'HttpServer
 * @param address IP server (NULL = "0.0.0.0")
 * @param port Porta del server
 * @return Se non ci sono stati errori ritorna 0
*/
int http_server_init(struct HttpServer* this, const char address[], uint16_t port);

/** Aggiunger un handler al server 
 * @param this Istanza dell'HttpServer
 * @param url Url di match [max HTTP_MAX_URL_SIZE]
 * @param callback Funzione da chiamare in caso di match
 * @param data Puntatore ad un'allocazione di memoria definita dall'utente (Questo puntatore verrà passato come parametro al callback)
 * @return Se non ci sono stati errori ritorna 0
*/
int http_server_add_handler(struct HttpServer* this, const char url[], HttpCallback callback, void* data);

/** Avvia il server 
 * @note Funzione bloccante, non termina finche il server è in esecuzione
 * @param this Istanza dell'HttpServer
 * @return Se non ci sono stati errori ritorna 0
*/
int http_server_run(struct HttpServer* this);

/** Termina forzatamente il server
 * @param this Istanza dell'HttpServer
 * @return Se non ci sono stati errori ritorna 0
 */
int http_server_stop(struct HttpServer* this);

#ifdef __cplusplus
}
#endif
#endif
