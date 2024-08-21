#include "http_server.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

/*** PRIVATE *******************************************************************/

/* Struttura per il contesto della richiesta */
struct HttpRequest {
    const struct HttpHandler* handlers; /* array con tutti gli handlers */
    HttpCallback callback; /* funzione da chiamare in caso di match */
    void* data; /* puntatore a memoria dati definita dall'utente in caso di match */
    int socket; /* Descrittore del socket della comunicazione tcp */
};

/** Funzione chiamata quando il parser trova un url
 * @param parser Istanza http_parser
 * @param at indirizzo inizio stringa url
 * @param length Lunghezza stringa url
 */
static int http_parser_on_url(http_parser* parser, const char *at, size_t length);

/** Funzione del server 
 * @param arg struct HttpServer*
*/
static void* http_server_run(void* arg);

/** Funzione del worker 
 * @param arg struct HttpServer*
*/
static void* http_worker_run(void* arg);

/** Funzione di cleanup del server 
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
        if(ret<0) return -1;
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
                inet_pton(AF_INET, address ? address : "0.0.0.0", &this->_addr.sin_addr);
                this->_addr.sin_port = htons(port);

                // Aggancio del socket all'indirizzo
                if (bind(this->_listener, (struct sockaddr *)&this->_addr, sizeof(this->_addr)) == 0){
                    // Creo la lista in entrata
                    if (listen(this->_listener, FD_SETSIZE - 2) == 0){
                        // Aggiungo il listener al set
                        FD_SET(this->_listener, &this->_master_set);
                        // Aggiorno lo stato del server
                        this->_state = HTTP_SERVER_INITIALIZED;
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
    if(this->_state != HTTP_SERVER_RUNNING) return -1;
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

    // se mi è rimasto un socket pendente lo riaggiungo al set
    if(this->_data != NULL) FD_SET(this->_data->socket, &this->_master_set);
    this->_data = NULL;

    // Indico che il server si stà spegnendo
    this->_state = HTTP_SERVER_STOPPING;
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
    this->_state = HTTP_SERVER_STOPPED;
}

static void* http_worker_run(void* arg){
    struct HttpServer* this = (struct HttpServer*)arg;
    struct sockaddr_in addr; /* Indirizzo client */
    struct HttpRequest request; /* Contesto della richiesta */
    int ret; /* Valore di ritorno delle funzioni */
    int state; /* Stato del thread */

    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &state);

    while(1){
        
        if((ret = pthread_mutex_lock(&this->_mutex_sync)) != 0) {
            fprintf(stderr, "%s:%d pthread_mutex_lock error: %s\r\n", __FILE__, __LINE__, strerror(ret));
        }
        // Attendo di ricevere un nuovo job
        while(this->_data == NULL && this->_state != HTTP_SERVER_STOPPING){
            if((ret = pthread_cond_wait(&this->_cond_sync, &this->_mutex_sync)) != 0){
                fprintf(stderr, "%s:%d pthread_cond_wait error: %s\r\n", __FILE__, __LINE__, strerror(ret));
            }
        }

        // Se i server si stà spegnendo
        if(this->_state == HTTP_SERVER_STOPPING){
            if((ret = pthread_mutex_unlock(&this->_mutex_sync)) != 0){
                fprintf(stderr, "%s:%d pthread_mutex_unlock error: %s\r\n", __FILE__, __LINE__, strerror(ret));
            }
            break;
        }else{
            memcpy(&request, this->_data, sizeof(request));
            this->_data = NULL;
            if((ret = pthread_mutex_unlock(&this->_mutex_sync)) != 0){
                fprintf(stderr, "%s:%d pthread_mutex_unlock error: %s\r\n", __FILE__, __LINE__, strerror(ret));
            }

            if((ret = pthread_cond_broadcast(&this->_cond_sync)) != 0){
                fprintf(stderr, "%s:%d pthread_cond_broadcast error: %s\r\n", __FILE__, __LINE__, strerror(ret));
            }
            // Eseguo la callback
            request.callback(request.socket, request.data);

            // Controllo se il socket è stato chiuso erroneamente nella callback
            if(fcntl(request.socket, F_GETFL) != -1 || errno != EBADF){
                if(getpeername(request.socket, (struct sockaddr *)&addr, &(socklen_t){sizeof(addr)}) == 0){
                    printf("%s:%d \x1b[31mdisconnected\x1b[0m\r\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
                } else fprintf(stderr, "%s:%d getpeername error: %s\r\n", __FILE__, __LINE__, strerror(errno));

                if(close(request.socket) != 0) {
                    fprintf(stderr, "%s:%d close error: %s\r\n", __FILE__, __LINE__, strerror(errno));
                }
            } else {
                fprintf(stderr, "worker warning: socket [%d] closed on callback.\r\n", request.socket);
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
    struct HttpRequest request; /* Contesto richiesta */
    int fd; /*Descrittore del Socket richiesta tcp */
    fd_set temp_set; /* Set che entra nella select */
    int i; /* Indice */
    int ret; /* Valore di ritorno delle funzioni */
    int state; /* Stato del thread */

    pthread_cleanup_push(http_server_cleanup, this);

    // inizializzo i setting del parser
    http_parser_settings_init(&settings);
    settings.on_url = http_parser_on_url;

    // Pulisco il set
    FD_ZERO(&temp_set);

    while(1){
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

                    // Attendo che il contesto della richiesta sia stato prelevato
                    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &state);
                    if((ret = pthread_mutex_lock(&this->_mutex_sync)) != 0){
                        fprintf(stderr, "%s:%d pthread_mutex_lock error: %s\r\n", __FILE__, __LINE__, strerror(ret));
                        break;
                    }
                    while(this->_data != NULL){
                        if((ret = pthread_cond_wait(&this->_cond_sync, &this->_mutex_sync)) != 0){
                            fprintf(stderr, "%s:%d pthread_cond_wait error: %s\r\n", __FILE__, __LINE__, strerror(ret));
                            break;
                        }
                    }   
                    if((ret = pthread_mutex_unlock(&this->_mutex_sync)) != 0){
                        fprintf(stderr, "%s:%d pthread_mutex_unlock error: %s\r\n", __FILE__, __LINE__, strerror(ret));
                        break;
                    }    
                    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &state);

                    // Pulisco il contesto della richiesta
                    memset(&request, 0, sizeof(request));
                    request.handlers = this->_handlers;
                    request.socket = fd;

                    // inizializzo il parser
                    http_parser_init(&parser, HTTP_REQUEST);
                    // passo il buffer dei dati al parser
                    parser.data = (void*)&request;

                    // ricevo i dati senza toglierli dallo stream
                    recved = recv(fd, buffer, sizeof(buffer), MSG_PEEK);

                    // Controllo se la connessione è stata chiusa o c'è stato un errore
                    if(recved > 0){
                        http_parser_execute(&parser, &settings, buffer, recved);

                        // Se c'è stato un errore
                        if(HTTP_PARSER_ERRNO(&parser)){
                            char response[512];
                            fprintf(
                                stderr, "%s:%d [%s] %s\r\n", 
                                inet_ntoa(addr.sin_addr), 
                                ntohs(addr.sin_port), 
                                http_errno_name(HTTP_PARSER_ERRNO(&parser)),
                                http_errno_description(HTTP_PARSER_ERRNO(&parser))
                            );
                            ret = snprintf(
                                response, 
                                sizeof(response),
                                "HTTP/1.1 500 Internal Server Error\r\n"
                                "Content-Type: text/html; charset=utf-8\r\n"
                                "Connection: close\r\n"
                                "Content-Length: %ld\r\n"
                                "\r\n"
                                "[%s] %s",
                                strlen(http_errno_name(HTTP_PARSER_ERRNO(&parser))) + strlen(": ") + strlen(http_errno_description(HTTP_PARSER_ERRNO(&parser))),
                                http_errno_name(HTTP_PARSER_ERRNO(&parser)),
                                http_errno_description(HTTP_PARSER_ERRNO(&parser))
                            );
                            // invio la risposta di errore
                            if(send(fd, response, ret, 0) < 0){
                                fprintf(stderr, "%s:%d send error: %s\r\n", __FILE__, __LINE__, strerror(errno));
                                break;
                            } 
                        } else 
                        // Se non è stato trovato nessun handler
                        if(request.callback == NULL){
                            char response[] = 
                                "HTTP/1.1 404 Not Found\r\n"
                                "Content-Type: text/html; charset=utf-8\r\n"
                                "Connection: close\r\n"
                                "Content-Length: 0\r\n"
                                "\r\n";
                            // invio la risposta di errore
                            if(send(fd, response, sizeof(response)-1, 0) < 0){
                                fprintf(stderr, "%s:%d send error: %s\r\n", __FILE__, __LINE__, strerror(errno));
                                break;
                            } 
                        } else {
                            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &state);
                            // Attendo lo slot per il contesto sia libero
                            if((ret = pthread_mutex_lock(&this->_mutex_sync)) != 0){
                                fprintf(stderr, "%s:%d pthread_mutex_lock error: %s\r\n", __FILE__, __LINE__, strerror(ret));
                                break;
                            }
                            while(this->_data != NULL){
                                if((ret = pthread_cond_wait(&this->_cond_sync, &this->_mutex_sync)) != 0){
                                    fprintf(stderr, "%s:%d pthread_cond_wait error: %s\r\n", __FILE__, __LINE__, strerror(ret));
                                    break;
                                }
                            }
                            // Aggiungo il nuovo contesto della richiesta da processare
                            this->_data = &request;
                            // rimuovo il socket dal set
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
                            // Il socket verrà chiuso all'interno del worker
                            continue;
                        }
                    } else if(recved < 0){
                        fprintf(stderr, "%s:%d recv error: %s\r\n", __FILE__, __LINE__, strerror(errno));
                        break;
                    } 

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
    if(this->_state != HTTP_SERVER_INITIALIZED) return -1;

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
            this->_state = HTTP_SERVER_RUNNING;
            return 0;
        }else fprintf(stderr, "%s:%d pthread_create error: %s\r\n", __FILE__, __LINE__, strerror(ret));
    }else{
        if((ret = pthread_mutex_lock(&this->_mutex_sync)) != 0){
            fprintf(stderr, "%s:%d pthread_mutex_lock error: %s\r\n", __FILE__, __LINE__, strerror(ret));
        }
        // Indico che il server si stà spegnendo
        this->_state = HTTP_SERVER_STOPPING;
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
    if(this->_state == HTTP_SERVER_INITIALIZED) return -1;

    if((ret = pthread_join(this->_thread, &tmp)) == 0){
        memset(this, 0, sizeof(*this));
        return 0;
    } else fprintf(stderr, "%s:%d pthread_join error: %s\r\n", __FILE__, __LINE__, strerror(ret));
    return -1;
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
    struct HttpRequest* requests = (struct HttpRequest*)parser->data; /* Contesto */
    struct sockaddr_in addr; /* Indirizzo client */
    struct http_parser_url parser_url; /* Url parser */
    int ret; /* Valore di ritorno */

    // inizializzo l'url parser
    http_parser_url_init(&parser_url);
    ret = http_parser_parse_url(at, length, false, &parser_url);
    if(ret != 0) return -1;
    
    // Stampo la richiesta ricevuto dal client
    if(getpeername(requests->socket, (struct sockaddr *)&addr, &(socklen_t){sizeof(addr)}) == 0){
        printf("%s:%d %s %.*s\r\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), http_method_str(parser->method), (int)length, at);
    } else fprintf(stderr, "%s:%d getpeername error: %s\r\n", __FILE__, __LINE__, strerror(errno));
    
    // scorro tutti gli handler
    for(int i=0; i<HTTP_MAX_HANDLERS; i++){
        // se lo slot è valido
        if(requests->handlers[i]._callback != NULL){
            if(strlen(requests->handlers[i]._url) != parser_url.field_data[UF_PATH].len) continue;
            ret = strncmp(requests->handlers[i]._url, at + parser_url.field_data[UF_PATH].off, parser_url.field_data[UF_PATH].len);
            if(ret != 0) continue;
            // aggiorno il contesto della callback
            requests->callback = requests->handlers[i]._callback;
            requests->data = requests->handlers[i]._data;
            return 0;
        }
    }
    return 0;
}

