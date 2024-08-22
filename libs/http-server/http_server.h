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

/* GLOBAL DEFINITION *****************************************************************/

#ifndef HTTP_MAX_HANDLERS
/** Numero massimo di handlers */
#define HTTP_MAX_HANDLERS 128
#endif

#ifndef HTTP_MAX_WORKERS
/** Numero massimo di handlers */
#define HTTP_MAX_WORKERS 4
#endif

/* UTILITY FUNCTIONS ************************************************************************/

/** 
 * @brief Importa un file all'interno del codice.
 * @param file Percorso del file da includere
 * @param sym Simbolo da usare per creare il puntatore al file 
 * @return sym (indirizzo di memoria) e _sizeof_sym (grandezza del file)
 * @note Se in uso lo standard c23, allora è consigliato utilizzare la direttiva `#embed`.
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
extern __attribute__((aligned(16))) __attribute__((nonstring)) const char sym[]

/** 
 * @brief Invia una risposta HTTP/1.1
 * @param socket Socket tcp dove inviare i dati
 * @param status Stato HTTP della risposta
 * @param header Header della risposta (opzionale, se NULL non viene incluso)
 * @param content Dati del body da inviare (opzionale, se NULL non viene incluso)
 * @param content_lenght Dimenzione del content
 * @return 0 se non ci sono stati errori
 */
int send_http_response(int socket, enum http_status status, const char* header, const char* content, size_t content_lenght);

/* HTTP SERVER TYPES ***************************************************************************/

/** 
 * @brief Funzione di callback
 * @param socket File Descriptor del socket
 * @param data puntatore a memoria dati definita dall'utente
 * @return Ritorna 0 se non ci sono stati errori
 * @warning È possibile che i dati puntati da data vengano letti/modificati da più thread
 */
typedef int(*HttpCallback)(int socket, void* data);

/** Struttura utilizzata per morizzare tutti gli handler */
struct HttpHandler {
    /* PRIVATE */
    HttpCallback _callback; /* funzione da chiamare in caso di match */
    const char* _url; /* url di match*/
    void* _data; /* puntatore a memoria dati definita dall'utente */
};

/** Stati del server  */
enum HttpServerState {
    HTTP_SERVER_STOPPED, /* Server fermo */
    HTTP_SERVER_INITIALIZED, /* Server inizializzato */
    HTTP_SERVER_RUNNING, /* Server in esecuzione */
    HTTP_SERVER_STOPPING, /* Server in spegnimento */
};

/* HTTP SERVER FUNCTIONS ***********************************************************************************/

/** Struttura del server */
struct HttpServer {
    /* PRIVATE */
    pthread_t _thread; /* Thread del server */
    int _listener; /* Socket per l'ascolto */
    struct sockaddr_in _addr; /* Indirizzo server */
    enum HttpServerState _state; /* Indica lo stato del server */
    fd_set _master_set; /* Set con tutti i descrittori */
    struct HttpHandler _handlers[HTTP_MAX_HANDLERS]; /* lista degli handler */
    pthread_t _workers[HTTP_MAX_WORKERS]; /* thread dei worker */
    struct HttpRequest* _data; /* Puntatore per lo scambio di dati tra i thread */
    pthread_mutex_t _mutex_sync; /* Mutex di sincronizzazione */
    pthread_cond_t _cond_sync; /* Varaibile cond di sincronizzazione */
};

/** 
 * @brief Inizializza la struttura Server 
 * @param this Istanza dell'HttpServer
 * @param address IP server (NULL = "0.0.0.0")
 * @param port Porta del server
 * @return Se non ci sono stati errori ritorna 0
*/
int http_server_init(struct HttpServer* this, const char address[], uint16_t port);

/** 
 * @brief Aggiunge un handler al server 
 * @param this Istanza dell'HttpServer
 * @param url Url di match
 * @param callback Funzione da chiamare in caso di match
 * @param data Puntatore ad un'allocazione di memoria definita dall'utente (Questo puntatore verrà passato come parametro al callback)
 * @return Se non ci sono stati errori ritorna 0
*/
int http_server_add_handler(struct HttpServer* this, const char* url, HttpCallback callback, void* data);

/** 
 * @brief Avvia il server 
 * @param this Istanza dell'HttpServer
 * @return Se non ci sono stati errori ritorna 0
*/
int http_server_start(struct HttpServer* this);

/** 
 * @brief Attende che il server sia terminato
 * @param this Istanza dell'HttpServer
 * @return Se non ci sono stati errori ritorna 0
*/
int http_server_join(struct HttpServer* this);

/** 
 * @brief Termina forzatamente il server
 * @param this Istanza dell'HttpServer
 * @return Se non ci sono stati errori ritorna 0
 */
int http_server_stop(struct HttpServer* this);

#ifdef __cplusplus
}
#endif
#endif
