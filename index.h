#ifndef _INDEX_H_
#define _INDEX_H_

/** Callback per ritornare la pagina index */
int html_index_callback(int socket, void* data);
/** Callback per ritornare il css per la pagina index */
int css_style_callback(int socket, void* data);
/** Callback per ritornare il js per la pagina index */
int js_script_callback(int socket, void* data);
/** Callback per ritornare l'icona per tutte le pagine */
int ico_favicon_callback(int socket, void* data);
/** Chiude il server e invia una risposta http di notifica */
int close_callback(int socket, void* data);
/** Invia un'immagine svg tramite http dopo aver atteso un tempo casuale */
int lazy_image_callback(int socket, void* data);
/** Autenticazione tramite Basic Auth */
int auth_callback(int socket, void* data);
/** Callback che restituisce nel bosy l'url della richiesta */
int url_callback(int socket, void* data);

#endif
