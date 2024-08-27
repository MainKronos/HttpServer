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

/**
 * @defgroup Http_Server
 * @brief Modulo che gestisce le connessioni http in entrata/uscita
 */

/**
 * @defgroup Http_Utility
 * @brief Modulo contenente funzioni di utilità per la gestione delle richieste/rispose http
 */

/* GLOBAL DEFINITION *****************************************************************/

#ifndef HTTP_MAX_HANDLERS
/** 
 * @ingroup Http_Server 
 * @brief Numero massimo di handlers 
 */
#define HTTP_MAX_HANDLERS 128
#endif

#ifndef HTTP_MAX_WORKERS
/** 
 * @ingroup Http_Server
 * @brief Numero massimo di worker 
 */
#define HTTP_MAX_WORKERS 4
#endif

/* UTILITY FUNCTIONS ************************************************************************/

/** 
 * @ingroup Http_Utility
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
 * @ingroup Http_Utility
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
 * @ingroup Http_Server
 * @brief Funzione di callback
 * @param socket File Descriptor del socket
 * @param data puntatore a memoria dati definita dall'utente
 * @return 0 se non ci sono stati errori
 * @warning È possibile che i dati puntati da `data` vengano letti/modificati da più thread
 */
typedef int(*HttpCallback)(int socket, void* data);

/** Struttura utilizzata per memorizzare tutti gli handler */
struct HttpHandler {
    /** @privatesection */

    /** metodo http di match */
    enum http_method method;
    /** url di match*/
    const char* url; 
    /** funzione da chiamare in caso di match */
    HttpCallback callback;
    /** puntatore a memoria dati definita dall'utente */
    void* data; 
};

/** Stato della richiesta http */
enum HttpRequestState {
    /** @privatesection */

    /** Slot richiesta vuoto */
    HTTP_WORKER_REQUEST_EMPTY,
    /** Richiesta pronta per essere processata */
    HTTP_WORKER_REQUEST_READY,
    /** Richiesta in esecuzione */
    HTTP_WORKER_REQUEST_RUNNING
};

/** 
 * @ingroup Http_Server
 * @brief Stati del server 
 */
enum HttpServerState {
    /** Server fermo */
    HTTP_SERVER_STOPPED,
    /** Server inizializzato */
    HTTP_SERVER_INITIALIZED,
    /** Server in esecuzione */
    HTTP_SERVER_RUNNING,
    /** Server in spegnimento */
    HTTP_SERVER_STOPPING
};

/** Struttura per il contesto della richiesta */
struct HttpRequest {
    /** @privatesection */

    /** Descrittore del socket della comunicazione tcp */
    int socket; 
    /** Stato della richiesta */
    enum HttpRequestState state;
    union {
        /** lista degli handler */
        const struct HttpHandler* handlers; 
        /** handler che ha fatto match */
        const struct HttpHandler* handler; 
    } ptr;
};


/* HTTP SERVER FUNCTIONS ***********************************************************************************/

/** 
 * @ingroup Http_Server
 * @brief Struttura del server 
 */
struct HttpServer {
    /* READ-ONLY */

    /** Indica lo stato del server */
    enum HttpServerState state; 

    /* PRIVATE */

    /** Thread del server @private */
    pthread_t _thread;
    /** Socket per l'ascolto @private */
    int _listener;
    /** Indirizzo server @private */
    struct sockaddr_in _addr; 
    /** Set con tutti i descrittori @private */
    fd_set _master_set; 
    /** lista degli handler @private */
    struct HttpHandler _handlers[HTTP_MAX_HANDLERS]; 
    /** thread dei worker @private */
    pthread_t _workers[HTTP_MAX_WORKERS]; 
    /** Jobs in esecuzione dai worker @private */
    struct HttpRequest _requests[HTTP_MAX_WORKERS]; 
    /** Mutex di sincronizzazione @private */
    pthread_mutex_t _mutex_sync; 
    /** Varaibile cond di sincronizzazione @private */
    pthread_cond_t _cond_sync; 
};

/** 
 * @ingroup Http_Server
 * @brief Inizializza la struttura Server 
 * @param this Istanza dell'HttpServer
 * @param address IP server (NULL = "0.0.0.0")
 * @param port Porta del server
 * @return 0 se non ci sono stati errori
 * @note Funzione NON bloccante
*/
int http_server_init(struct HttpServer* this, const char address[], uint16_t port);

/** 
 * @ingroup Http_Server
 * @brief Aggiunge un handler al server 
 * @param this Istanza dell'HttpServer
 * @param method Metodo http di match
 * @param url Url di match
 * @param callback Funzione da chiamare in caso di match
 * @param data Puntatore ad un'allocazione di memoria definita dall'utente (Questo puntatore verrà passato come parametro al callback)
 * @return 0 se non ci sono stati errori
 * @note Funzione NON bloccante
*/
int http_server_add_handler(struct HttpServer* this, enum http_method method, const char* url, HttpCallback callback, void* data);

/** 
 * @ingroup Http_Server
 * @brief Avvia il server 
 * @param this Istanza dell'HttpServer
 * @return 0 se non ci sono stati errori
 * @note Funzione NON bloccante
*/
int http_server_start(struct HttpServer* this);

/** 
 * @ingroup Http_Server
 * @brief Termina forzatamente il server
 * @param this Istanza dell'HttpServer
 * @return 0 se non ci sono stati errori
 * @note Funzione NON bloccante
 */
int http_server_stop(struct HttpServer* this);

/** 
 * @ingroup Http_Server
 * @brief Attende che il server sia terminato
 * @param this Istanza dell'HttpServer
 * @return 0 se non ci sono stati errori
 * @note Funzione bloccante
*/
int http_server_join(struct HttpServer* this);

#ifdef __cplusplus
}
#endif
#endif
