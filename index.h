#ifndef _INDEX_H_
#define _INDEX_H_

#include <stdlib.h>
#include <http_server.h>

int html_index_callback(int socket, void* data);
int css_style_callback(int socket, void* data);
int js_script_callback(int socket, void* data);
int ico_favicon_callback(int socket, void* data);

/* Chiude il server e invia una risposta http di notifica */
int close_callback(int socket, void* data);

/* Invia un'immagine svg tramite http dopo aver atteso un tempo casuale */
int lazy_image_callback(int socket, void* data);

/* Invia tramite http la pagina con tutti i test */
int html_test_callback(int socket, void* data);

int test0_callback(int socket, void* data);
int test1_callback(int socket, void* data);
int test2_callback(int socket, void* data);
int test3_callback(int socket, void* data);
int test4_callback(int socket, void* data);
int test5_callback(int socket, void* data);
int test6_callback(int socket, void* data);
int test7_callback(int socket, void* data);
#endif
