#include "http_server.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

/*** PRIVATE *******************************************************************/

/** 
 * @brief Funzione chiamata quando il parser trova un url
 * @param parser Istanza http_parser
 * @param at indirizzo inizio stringa url
 * @param length Lunghezza stringa url
 */
static int http_parser_on_url(http_parser* parser, const char *at, size_t length);

/** 
 * @brief Funzione del server 
 * @param arg struct HttpServer*
*/
static void* http_server_run(void* arg);

/** 
 * @brief Funzione del worker 
 * @param arg struct HttpServer*
*/
static void* http_worker_run(void* arg);

/** 
 * @brief Funzione di cleanup del server 
 * @param arg struct HttpServer*
*/
static void http_server_cleanup(void* arg);

/******************************************************************************/

int send_http_response(int socket, enum http_status status, const char* header, const char* content, size_t content_lenght){
    char* buffer; /* Buffer per l'header */
    size_t size; /* Content size*/
    size_t sent; /* Byte inviati */
    int ret; /* Retorno funzioni */

    /* Format dell'header per la sprintf */
    const char header_format[] = 
        "HTTP/1.1 %d %s\r\n"
        "Content-Length: %ld\r\n"
        "%s\r\n";

    if(header == NULL) header = "";

    ret = snprintf(
        NULL, 0,
        header_format,
        status,
        http_status_str(status),
        content_lenght,
        header
    );

    if(ret < 0) return -1;
    size =+ ret + 1; // ret + '\0'

    if((buffer = malloc(size)) == NULL){
        fprintf(stderr, "%s:%d malloc error: %s\r\n", __FILE__, __LINE__, strerror(errno));
        return -1;
    }

    ret = snprintf(
        buffer, size,
        header_format,
        status,
        http_status_str(status),
        content_lenght,
        header
    );

    if(ret < 0){
        free(buffer);
        return -1;
    }

    size = ret;

    // Invio l'header
    sent = 0;
    do {
        ret = send(socket, buffer + sent, size - sent, 0);
        if(ret < 0){
            free(buffer);
            return -1;
        } 
        sent += ret;
    } while (sent < size);

    free(buffer);

    if(content == NULL || content_lenght == 0) return 0;

    // Invio il body
    sent = 0;
    do {
        ret = send(socket, content + sent, content_lenght - sent, 0);
        if(ret < 0) return -1;
        sent += ret;
    } while (sent < content_lenght);

    return 0;
}

int http_server_init(struct HttpServer* this, const char address[], uint16_t port){
    // Check iniziali
    if(this == NULL) return -1;
    if(port <= 0) return -1;

    // Pulisco la struttura
    memset(this, 0, sizeof(*this));

    // pulisco il set
    FD_ZERO(&this->_master_set);

    // Inizializzazione mutex
    if(pthread_mutex_init(&this->_mutex_sync, NULL) == 0){
        // Inizializzazione cond
        if(pthread_cond_init(&this->_cond_sync, NULL) == 0){
            // Creazione socket
            if((this->_listener = socket(AF_INET, SOCK_STREAM, 0)) >= 0){
                if(setsockopt(this->_listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) != 0){
                    fprintf(stderr, "%s:%d setsockopt error: %s\r\n", __FILE__, __LINE__, strerror(errno));
                }

                // Creazione indirizzo
                memset(&this->_addr, 0, sizeof(this->_addr));
                this->_addr.sin_family = AF_INET;
                if(address == NULL){
                    this->_addr.sin_addr.s_addr = INADDR_ANY;
                } else {
                    inet_pton(AF_INET, address, &this->_addr.sin_addr);
                }
                this->_addr.sin_port = htons(port);

                // Aggancio del socket all'indirizzo
                if (bind(this->_listener, (struct sockaddr *)&this->_addr, sizeof(this->_addr)) == 0){
                    // Creo la lista in entrata
                    if (listen(this->_listener, FD_SETSIZE - 2) == 0){
                        // Aggiungo il listener al set
                        FD_SET(this->_listener, &this->_master_set);
                        // Aggiorno lo stato del server
                        this->state = HTTP_SERVER_INITIALIZED;
                        return 0;
                    } else fprintf(stderr, "%s:%d listen error: %s\r\n", __FILE__, __LINE__, strerror(errno));
                } else fprintf(stderr, "%s:%d bind error: %s\r\n", __FILE__, __LINE__, strerror(errno));
                close(this->_listener);
            } else fprintf(stderr, "%s:%d socket error: %s\r\n", __FILE__, __LINE__, strerror(errno));
            pthread_cond_destroy(&this->_cond_sync);
        } else fprintf(stderr, "%s:%d pthread_cond_init error: %s\r\n", __FILE__, __LINE__, strerror(errno));
        pthread_mutex_destroy(&this->_mutex_sync);
    } else fprintf(stderr, "%s:%d pthread_mutex_init error: %s\r\n", __FILE__, __LINE__, strerror(errno));
    return -1;
}

int http_server_stop(struct HttpServer* this){
    int ret;
    // Check iniziali
    if(this == NULL) return -1;
    if(this->state != HTTP_SERVER_RUNNING) return -1;
    if(this->_listener < 0) return -1;

    printf("%s:%d stopping...\r\n", inet_ntoa(this->_addr.sin_addr), ntohs(this->_addr.sin_port));
    fflush(stdout);
    if((ret = pthread_cancel(this->_thread)) != 0){
        fprintf(stderr, "%s:%d pthread_cancel error: %s\r\n", __FILE__, __LINE__,  strerror(ret));
        return -1;
    }
    return 0;
}

static void http_server_cleanup(void* arg){
    struct HttpServer* this = (struct HttpServer*)arg;
    void* tmp; /* variabile per il join */
    struct sockaddr_in addr; /* Indirizzo client */
    int ret;

    FD_CLR(this->_listener, &this->_master_set);

    if(close(this->_listener) != 0){
        fprintf(stderr, "%s:%d close error: %s\r\n", __FILE__, __LINE__, strerror(errno));
    }

    if((ret = pthread_mutex_lock(&this->_mutex_sync)) != 0){
        fprintf(stderr, "%s:%d pthread_mutex_lock error: %s\r\n", __FILE__, __LINE__, strerror(ret));
    }

    // se mi sono rimasti socket pendenti li riaggiungo al set
    for(int slot = 0; slot < HTTP_MAX_WORKERS; slot++){
        if(this->_requests[slot].state == HTTP_WORKER_REQUEST_READY){
            FD_SET(this->_requests[slot].socket, &this->_master_set);
            this->_requests[slot].state = HTTP_WORKER_REQUEST_EMPTY;
        }
    }

    // Indico che il server si stà spegnendo
    this->state = HTTP_SERVER_STOPPING;
    if((ret = pthread_mutex_unlock(&this->_mutex_sync)) != 0){
        fprintf(stderr, "%s:%d pthread_mutex_unlock error: %s\r\n", __FILE__, __LINE__, strerror(ret));
    }
    // Risveglio tutti i worker
    if((ret = pthread_cond_broadcast(&this->_cond_sync)) != 0){
        fprintf(stderr, "%s:%d pthread_cond_broadcast error: %s\r\n", __FILE__, __LINE__, strerror(ret));
    }

    // Chiudo tutti i socket nel set
    for(int i=0; i<FD_SETSIZE; i++){
        if(FD_ISSET(i, &this->_master_set)){
            if(getpeername(i, (struct sockaddr *)&addr, &(socklen_t){sizeof(addr)}) == 0){
                printf("%s:%d \x1b[31mdisconnected\x1b[0m\r\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
            } else fprintf(stderr, "%s:%d getpeername error: %s\r\n", __FILE__, __LINE__, strerror(errno));
            if(close(i) != 0) {
                fprintf(stderr, "%s:%d, close error: %s\r\n", __FILE__, __LINE__, strerror(errno));
            }
        }
    }

    // Attendo che i worker abbiano terminato
    for(int i=0; i<HTTP_MAX_WORKERS; i++){
        if((ret = pthread_join(this->_workers[i], &tmp)) != 0){
            fprintf(stderr, "%s:%d pthread_join error: %s\r\n", __FILE__, __LINE__, strerror(ret));
        }
    }

    if((ret = pthread_cond_destroy(&this->_cond_sync)) != 0){
        fprintf(stderr, "%s:%d pthread_cond_destroy error: %s\r\n", __FILE__, __LINE__, strerror(ret));
    }
    if((ret = pthread_mutex_destroy(&this->_mutex_sync)) != 0){
        fprintf(stderr, "%s:%d pthread_mutex_destroy error: %s\r\n", __FILE__, __LINE__, strerror(ret));
    } 

    printf("%s:%d stopped\r\n", inet_ntoa(this->_addr.sin_addr), ntohs(this->_addr.sin_port));

    // Indico che il server si è fermato
    this->state = HTTP_SERVER_STOPPED;
}

static void* http_worker_run(void* arg){
    struct HttpServer* this = (struct HttpServer*)arg;
    struct sockaddr_in addr; /* Indirizzo client */
    struct HttpRequest* request; /* Contesto della richiesta */
    int ret; /* Valore di ritorno delle funzioni */
    int state; /* Stato del thread */

    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &state);

    while(true){
        // Resetto il puntatore
        request = NULL;

        if((ret = pthread_mutex_lock(&this->_mutex_sync)) != 0) {
            fprintf(stderr, "%s:%d pthread_mutex_lock error: %s\r\n", __FILE__, __LINE__, strerror(ret));
        }

        // Attendo di ricevere un nuova richiesta
        while(true){
            for(int slot = 0; slot < HTTP_MAX_WORKERS; slot++){
                if(this->_requests[slot].state == HTTP_WORKER_REQUEST_READY){
                    // mi salvo il puntatore
                    request = &this->_requests[slot];
                    break;
                } 
            }
            // se  ho trovato uno slot libero o il server si stà spegnendo
            if(request != NULL || this->state == HTTP_SERVER_STOPPING) break;

            // Attendo
            if((ret = pthread_cond_wait(&this->_cond_sync, &this->_mutex_sync)) != 0){
                fprintf(stderr, "%s:%d pthread_cond_wait error: %s\r\n", __FILE__, __LINE__, strerror(ret));
                break;
            }
        }

        // Se c'è stato un errore o il server si stà spegnendo
        if(request == NULL || this->state == HTTP_SERVER_STOPPING){
            if((ret = pthread_mutex_unlock(&this->_mutex_sync)) != 0){
                fprintf(stderr, "%s:%d pthread_mutex_unlock error: %s\r\n", __FILE__, __LINE__, strerror(ret));
            }
            break;
        }else{
            // Aggiorno lo stato della richiesta
            request->state = HTTP_WORKER_REQUEST_RUNNING;
            if((ret = pthread_mutex_unlock(&this->_mutex_sync)) != 0){
                fprintf(stderr, "%s:%d pthread_mutex_unlock error: %s\r\n", __FILE__, __LINE__, strerror(ret));
            }
            
            // Eseguo la callback
            request->ptr.handler->callback(request->socket, request->ptr.handler->data);

            // Controllo se il socket è stato chiuso erroneamente nella callback
            if(fcntl(request->socket, F_GETFL) != -1 || errno != EBADF){
                if(getpeername(request->socket, (struct sockaddr *)&addr, &(socklen_t){sizeof(addr)}) == 0){
                    printf("%s:%d \x1b[31mdisconnected\x1b[0m\r\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
                } else fprintf(stderr, "%s:%d getpeername error: %s\r\n", __FILE__, __LINE__, strerror(errno));

                if(close(request->socket) != 0) {
                    fprintf(stderr, "%s:%d close error: %s\r\n", __FILE__, __LINE__, strerror(errno));
                }
            } else {
                fprintf(stderr, "worker warning: socket [%d] closed on callback.\r\n", request->socket);
            }
            
            if((ret = pthread_mutex_lock(&this->_mutex_sync)) != 0) {
                fprintf(stderr, "%s:%d pthread_mutex_lock error: %s\r\n", __FILE__, __LINE__, strerror(ret));
            }

            // rilascio lo slot
            request->state = HTTP_WORKER_REQUEST_EMPTY;

            if((ret = pthread_mutex_unlock(&this->_mutex_sync)) != 0){
                fprintf(stderr, "%s:%d pthread_mutex_unlock error: %s\r\n", __FILE__, __LINE__, strerror(ret));
            }

            // Risveglio il server che potrebbe essere in attesa di uno slot libero
            if((ret = pthread_cond_broadcast(&this->_cond_sync)) != 0){
                fprintf(stderr, "%s:%d pthread_cond_broadcast error: %s\r\n", __FILE__, __LINE__, strerror(ret));
            }

        }
    }

    pthread_exit(NULL);
    return NULL;
}

static void* http_server_run(void* arg){
    struct HttpServer* this = (struct HttpServer*)arg;
    struct sockaddr_in addr; /* Indirizzo client */
    ssize_t recved; /* Bytes ricevuti */
    char buffer[HTTP_MAX_HEADER_SIZE]; /* Buffer per la richiesta */
    http_parser parser; /* Istanza http parser */
    http_parser_settings settings; /* Istanza settings del parser */
    int fd; /*Descrittore del Socket richiesta tcp */
    fd_set temp_set; /* Set che entra nella select */
    struct HttpRequest request; /* Contesto della richiesta */
    int i; /* Indice */
    int ret; /* Valore di ritorno delle funzioni */
    int state; /* Stato del thread */

    pthread_cleanup_push(http_server_cleanup, this);

    // inizializzo i setting del parser
    http_parser_settings_init(&settings);
    settings.on_url = http_parser_on_url;

    // Pulisco il set
    FD_ZERO(&temp_set);

    while(true){
        // Copio il set
        temp_set = this->_master_set;

        if(select(FD_SETSIZE, &temp_set, NULL, NULL, NULL) < 0){
            fprintf(stderr, "%s:%d select error: %s\r\n", __FILE__, __LINE__, strerror(errno));
            break;
        }

        for(i=0; i<FD_SETSIZE; i++){
            if(FD_ISSET(i, &temp_set)){
                // Se ho una nuova connessione
                if(i == this->_listener){
                    // Aggiungo il nuovo socket al set
                    if((fd = accept(this->_listener, (struct sockaddr *)&addr, &(socklen_t){sizeof(addr)})) >= 0 ){
                        FD_SET(fd, &this->_master_set);
                        printf("%s:%d \x1b[32mconnected\x1b[0m\r\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
                    } else{
                        fprintf(stderr, "%s:%d accept error: %s\r\n", __FILE__, __LINE__, strerror(errno));
                        break;
                    }
                } // se ho dei dati in ingresso
                else {
                    fd = i;

                    // ricevo i dati senza toglierli dallo stream
                    recved = recv(fd, buffer, sizeof(buffer), MSG_PEEK);

                    // Controllo se la connessione è stata chiusa o c'è stato un errore
                    if(recved > 0){
                        // Pulisco il contesto della richiesta
                        memset(&request, 0, sizeof(request));
                        request.ptr.handlers = this->_handlers;
                        request.socket = fd;

                        // inizializzo il parser
                        http_parser_init(&parser, HTTP_REQUEST);
                        // passo il buffer dei dati al parser
                        parser.data = (void*)&request;

                        http_parser_execute(&parser, &settings, buffer, recved);

                        // Se non c'è stata un errore, la versione http è supportata 
                        // ed è stato trovato un handler con lo stesso metodo della richiesta
                        if(HTTP_PARSER_ERRNO(&parser) == HPE_OK && parser.http_major == 1 && request.ptr.handler != NULL && request.ptr.handler->method == parser.method){
                            /** Slot libero per la richiesta */
                            int slot;

                            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &state);
                            // Attendo lo slot per il contesto sia libero
                            if((ret = pthread_mutex_lock(&this->_mutex_sync)) != 0){
                                fprintf(stderr, "%s:%d pthread_mutex_lock error: %s\r\n", __FILE__, __LINE__, strerror(ret));
                                break;
                            }

                            // Cerco uno slot libero
                            while(true){
                                for(slot = 0; slot < HTTP_MAX_WORKERS; slot++){
                                    if(this->_requests[slot].state == HTTP_WORKER_REQUEST_EMPTY) break;
                                }
                                // se non ho trovato uno slot libero
                                if(slot == HTTP_MAX_WORKERS){
                                    // Attendo
                                    if((ret = pthread_cond_wait(&this->_cond_sync, &this->_mutex_sync)) != 0){
                                        fprintf(stderr, "%s:%d pthread_cond_wait error: %s\r\n", __FILE__, __LINE__, strerror(ret));
                                        break;
                                    }
                                } else break;
                            };

                            // Se c'è stato un errore
                            if(slot == HTTP_MAX_WORKERS) break;

                            // Aggiorno la richiesta
                            request.state = HTTP_WORKER_REQUEST_READY;
                            memcpy(&this->_requests[slot], &request, sizeof(request));

                            // rimuovo il socket dal set perchè verrà chiuso all'interno del worker
                            FD_CLR(fd, &this->_master_set);
                            if((ret = pthread_mutex_unlock(&this->_mutex_sync)) != 0){
                                fprintf(stderr, "%s:%d pthread_mutex_unlock error: %s\r\n",__FILE__, __LINE__, strerror(ret));
                                break;
                            }
                            // Risveglio un thread bloccato
                            if((ret = pthread_cond_signal(&this->_cond_sync)) != 0){
                                fprintf(stderr, "%s:%d pthread_cond_broadcast error: %s\r\n", __FILE__, __LINE__, strerror(ret));
                                break;
                            }
                            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &state);
                        }else{
                            // svuoto il buffer perchè non mi serve
                            while((ret = recv(fd, buffer, sizeof(buffer), MSG_DONTWAIT) != -1 && errno != EWOULDBLOCK));

                            // Se c'è stato un errore
                            if(HTTP_PARSER_ERRNO(&parser)){
                                fprintf(
                                    stderr, "%s:%d [%s] %s\r\n", 
                                    inet_ntoa(addr.sin_addr), 
                                    ntohs(addr.sin_port), 
                                    http_errno_name(HTTP_PARSER_ERRNO(&parser)),
                                    http_errno_description(HTTP_PARSER_ERRNO(&parser))
                                );
                                snprintf(
                                    buffer, 
                                    sizeof(buffer),
                                    "HTTP/1.1 %d %s\r\n"
                                    "Content-Type: text/html; charset=utf-8\r\n"
                                    "Connection: keep-alive\r\n"
                                    "Content-Length: %ld\r\n"
                                    "\r\n"
                                    "[%s] %s",
                                    HTTP_STATUS_INTERNAL_SERVER_ERROR,
                                    http_status_str(HTTP_STATUS_INTERNAL_SERVER_ERROR),
                                    strlen(http_errno_name(HTTP_PARSER_ERRNO(&parser))) + strlen("[] ") + strlen(http_errno_description(HTTP_PARSER_ERRNO(&parser))),
                                    http_errno_name(HTTP_PARSER_ERRNO(&parser)),
                                    http_errno_description(HTTP_PARSER_ERRNO(&parser))
                                );
                            } else
                            // Se la richiesta usa un protocollo http non supportato
                            if(parser.http_major != 1) {
                                snprintf(
                                    buffer,
                                    sizeof(buffer),
                                    "HTTP/1.1 %d %s\r\n"
                                    "Content-Type: text/html; charset=utf-8\r\n"
                                    "Connection: keep-alive\r\n"
                                    "Content-Length: 0\r\n"
                                    "\r\n",
                                    HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED,
                                    http_status_str(HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED)
                                );
                            } else
                            // Se non è stato trovato nessun handler
                            if(request.ptr.handler == NULL) {
                                snprintf(
                                    buffer,
                                    sizeof(buffer),
                                    "HTTP/1.1 %d %s\r\n"
                                    "Content-Type: text/html; charset=utf-8\r\n"
                                    "Connection: keep-alive\r\n"
                                    "Content-Length: 0\r\n"
                                    "\r\n",
                                    HTTP_STATUS_NOT_FOUND,
                                    http_status_str(HTTP_STATUS_NOT_FOUND)
                                );
                            } else 
                            // Se è stato trovato un handler ma il metodo è diverso
                            if(request.ptr.handler->method != parser.method) {
                                snprintf(
                                    buffer,
                                    sizeof(buffer),
                                    "HTTP/1.1 %d %s\r\n"
                                    "Content-Type: text/html; charset=utf-8\r\n"
                                    "Connection: keep-alive\r\n"
                                    "Content-Length: 0\r\n"
                                    "\r\n",
                                    HTTP_STATUS_METHOD_NOT_ALLOWED,
                                    http_status_str(HTTP_STATUS_METHOD_NOT_ALLOWED)
                                );
                            } else {
                                // Errore critico non gestito
                                break;
                            }

                            // invio la risposta di errore
                            if(send(fd, buffer, strlen(buffer), 0) < 0){
                                fprintf(stderr, "%s:%d send error: %s\r\n", __FILE__, __LINE__, strerror(errno));
                                break;
                            }
                        }
                    } else // se la connesione è stata chiusa
                    if(recved == 0){
                        if(getpeername(fd, (struct sockaddr *)&addr, &(socklen_t){sizeof(addr)}) == 0){
                            printf("%s:%d \x1b[31mdisconnected\x1b[0m\r\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
                        } else fprintf(stderr, "%s:%d getpeername error: %s\r\n", __FILE__, __LINE__, strerror(errno));

                        // rimuovo il socket dal set
                        FD_CLR(fd, &this->_master_set);
                        // chiudo la connessione
                        if(close(fd) != 0){
                            fprintf(stderr, "%s:%d close error: %s\r\n", __FILE__, __LINE__, strerror(errno));
                            break;
                        }
                    } else // se c'e stato un errore
                    if(recved < 0){
                        fprintf(stderr, "%s:%d recv error: %s\r\n", __FILE__, __LINE__, strerror(errno));
                        break;
                    }
                }
            }
        }

        // Se c'è stato un errore
        if(i != FD_SETSIZE){
            break;
        }
    }

    pthread_exit(NULL);
    pthread_cleanup_pop(0);
    return NULL;
}

int http_server_start(struct HttpServer* this){
    int i;
    int ret;
    void* tmp;
    // Check iniziali
    if(this == NULL) return -1;
    if(this->state != HTTP_SERVER_INITIALIZED) return -1;

    for(i = 0; i < HTTP_MAX_WORKERS; i++){
        if((ret = pthread_create(&this->_workers[i], NULL, http_worker_run, (void*)this)) != 0){
            fprintf(stderr, "%s:%d pthread_create error: %s\r\n", __FILE__, __LINE__, strerror(ret));
            break;
        }
    }
    // Se sono stati creati tutti i worker
    if(i == HTTP_MAX_WORKERS){
        if((ret = pthread_create(&this->_thread, NULL, http_server_run, (void*)this)) == 0){
            printf("%s:%d started\r\n", inet_ntoa(this->_addr.sin_addr), ntohs(this->_addr.sin_port));
            this->state = HTTP_SERVER_RUNNING;
            return 0;
        }else fprintf(stderr, "%s:%d pthread_create error: %s\r\n", __FILE__, __LINE__, strerror(ret));
    }else{
        if((ret = pthread_mutex_lock(&this->_mutex_sync)) != 0){
            fprintf(stderr, "%s:%d pthread_mutex_lock error: %s\r\n", __FILE__, __LINE__, strerror(ret));
        }
        // Indico che il server si stà spegnendo
        this->state = HTTP_SERVER_STOPPING;
        if((ret = pthread_mutex_unlock(&this->_mutex_sync)) != 0){
            fprintf(stderr, "%s:%d pthread_mutex_unlock error: %s\r\n", __FILE__, __LINE__, strerror(ret));
        }
        // Risveglio tutti i worker
        if((ret = pthread_cond_broadcast(&this->_cond_sync)) != 0){
            fprintf(stderr, "%s:%d pthread_cond_broadcast error: %s\r\n", __FILE__, __LINE__, strerror(ret));
        }

        for(; i>=0; i--){
            if((ret = pthread_join(this->_workers[i], &tmp)) != 0){
                fprintf(stderr, "%s:%d pthread_join error: %s\r\n", __FILE__, __LINE__, strerror(ret));
            }
        }
    }

    if((ret = pthread_cond_destroy(&this->_cond_sync)) != 0){
        fprintf(stderr, "%s:%d pthread_cond_destroy error: %s\r\n", __FILE__, __LINE__, strerror(ret));
    }
    if((ret = pthread_mutex_destroy(&this->_mutex_sync)) != 0){
        fprintf(stderr, "%s:%d pthread_mutex_destroy error: %s\r\n", __FILE__, __LINE__, strerror(ret));
    }
    if(close(this->_listener) != 0){  
        fprintf(stderr, "%s:%d close error: %s\r\n", __FILE__, __LINE__, strerror(errno));
    } 
    memset(this, 0, sizeof(*this));
    return -1;
}

int http_server_join(struct HttpServer* this){
    int ret;
    void* tmp;

    // Check iniziali
    if(this == NULL) return -1;
    if(this->state == HTTP_SERVER_INITIALIZED) return -1;

    if((ret = pthread_join(this->_thread, &tmp)) == 0){
        memset(this, 0, sizeof(*this));
        return 0;
    } else fprintf(stderr, "%s:%d pthread_join error: %s\r\n", __FILE__, __LINE__, strerror(ret));
    return -1;
}

int http_server_add_handler(struct HttpServer* this, enum http_method method, const char* url, HttpCallback callback, void* data){
    // Check iniziali
    if(this == NULL) return -1;
    if(url == NULL) return -1;
    if(callback == NULL) return -1;
    if(this->state != HTTP_SERVER_INITIALIZED) return -1;

    for(int i=0; i<HTTP_MAX_HANDLERS; i++){
        // se ho trovato uno slot libero
        if(this->_handlers[i].callback == NULL && this->_handlers[i].url == NULL){
            this->_handlers[i].method = method;
            this->_handlers[i].url = url;
            this->_handlers[i].callback = callback;
            this->_handlers[i].data = data;
            return 0;
        }else if(this->_handlers[i].method == method && (this->_handlers[i].url == url || strcmp(this->_handlers[i].url, url) == 0)){
            // Errore logico
            // Non ha senso che la coppia metodo e url sia collegata a più funzioni
            // perchè non ci sarebbe modo di sapere quale chiamare
            // nel caso ci fosse un match
            return -1;
        }
    }
    return -1;
}

static int http_parser_on_url(http_parser* parser, const char *at, size_t length){
    struct HttpRequest* request = (struct HttpRequest*)parser->data; /* Contesto */
    struct sockaddr_in addr; /* Indirizzo client */
    struct http_parser_url parser_url; /* Url parser */
    int ret; /* Valore di ritorno */
    const struct HttpHandler* handler; /* Handler che ha fatto match */

    // inizializzo l'url parser
    http_parser_url_init(&parser_url);
    ret = http_parser_parse_url(at, length, false, &parser_url);
    if(ret != 0) return -1;
    
    // Stampo la richiesta ricevuto dal client
    if(getpeername(request->socket, (struct sockaddr *)&addr, &(socklen_t){sizeof(addr)}) == 0){
        printf("%s:%d %s %.*s\r\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), http_method_str(parser->method), (int)length, at);
    } else fprintf(stderr, "%s:%d getpeername error: %s\r\n", __FILE__, __LINE__, strerror(errno));
    
    handler = NULL;

    // scorro tutti gli handler
    for(int i=0; i<HTTP_MAX_HANDLERS; i++){
        // se lo slot è valido
        if(request->ptr.handlers[i].callback != NULL){
            if(strlen(request->ptr.handlers[i].url) != parser_url.field_data[UF_PATH].len) continue;
            ret = strncmp(request->ptr.handlers[i].url, at + parser_url.field_data[UF_PATH].off, parser_url.field_data[UF_PATH].len);
            if(ret != 0) continue;

            // mi salvo il puntatore per indicare che l'url ha fatto match
            handler = &request->ptr.handlers[i];

            // se il metodo è quello giusto mi fermo
            if(handler->method == parser->method) break;
        }
    }

    // aggiorno il puntatore
    request->ptr.handler = handler;
    return 0;
}

