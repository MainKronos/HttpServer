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

#include <http_parser.h>

#ifdef __linux__
#include <sys/socket.h>
#include <sys/select.h>
#else
#include <sockLib.h>
#include <hostLib.h>
#include <selectLib.h>
#endif
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <pthread.h>

#ifndef HTTP_MAX_URL_SIZE
/** Dimenzione massima dell'url */
#define HTTP_MAX_URL_SIZE 256
#endif

#ifndef HTTP_MAX_HANDLERS
/** Numero massimo di handlers */
#define HTTP_MAX_HANDLERS 128
#endif

#ifndef HTTP_MAX_WORKERS
/** Numero massimo di handlers */
#define HTTP_MAX_WORKERS 10
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
extern __attribute__((aligned(16))) __attribute__((nonstring)) const uint8_t sym[]

/** Funzione di callback
 * @param socket File Descriptor del socket
 * @param data puntatore a memoria dati definita dall'utente
 * @return Ritorna 0 per keep-alive altrimenti chiude la connessione
 */
typedef int(*HttpCallback)(int socket, void* data);

/* Struttura utilizzata per morizzare tutti gli handler */
struct HttpHandler {
    /* PRIVATE */
    HttpCallback _callback; /* funzione da chiamare in caso di match */
    const char* _url; /* url di match*/
    void* _data; /* puntatore a memoria dati definita dall'utente */
};

enum HttpServerState {
    HTTP_SERVER_STOPPED,
    HTTP_SERVER_INITIALIZED,
    HTTP_SERVER_RUNNING
};

/* Struttura del server */
struct HttpServer {
    /* PRIVATE */
    pthread_t _thread; /* Thread del server */
    int _listener; /* Socket per l'ascolto */
    struct sockaddr_in _addr; /* Indirizzo server */
    enum HttpServerState _state; /* Indica lo stato del server */
    fd_set _master_set; /* Set con tutti i descrittori */
    struct HttpHandler _handlers[HTTP_MAX_HANDLERS]; /* lista degli handler */
    pthread_t _workers[HTTP_MAX_WORKERS]; /* thread worker */
    struct HttpRequestCtx* _data; /* Puntatore per lo scambio di dati tra i thread */
    pthread_mutex_t _mutex_sync; /* Mutex di sincronizzazione */
    pthread_cond_t _cond_sync; /* Varaibile cond di sincronizzazione */
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
 * @param url Url di match
 * @param callback Funzione da chiamare in caso di match
 * @param data Puntatore ad un'allocazione di memoria definita dall'utente (Questo puntatore verrà passato come parametro al callback)
 * @return Se non ci sono stati errori ritorna 0
*/
int http_server_add_handler(struct HttpServer* this, const char* url, HttpCallback callback, void* data);

/** Avvia il server 
 * @param this Istanza dell'HttpServer
 * @return Se non ci sono stati errori ritorna 0
*/
int http_server_start(struct HttpServer* this);

/** Attende che il server sia terminato
 * @param this Istanza dell'HttpServer
 * @return Se non ci sono stati errori ritorna 0
*/
int http_server_join(struct HttpServer* this);

/** Termina forzatamente il server
 * @param this Istanza dell'HttpServer
 * @return Se non ci sono stati errori ritorna 0
 */
int http_server_stop(struct HttpServer* this);

#ifdef __cplusplus
}
#endif
#endif
