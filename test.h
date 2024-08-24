#ifndef _TESTS_H_
#define _TESTS_H_

/* Invia tramite http la pagina con tutti i test */
int html_test_callback(int socket, void* data);
/** Questo test usa una callback che preleva tutta i dati in arrivo dal socket */
int test0_callback(int socket, void* data);
/** Questo test usa una callback che non preleva dati in arrivo dal socket */
int test1_callback(int socket, void* data);
/** Questo test usa una callback che preleva parzialmente [10 byte] i dati in arrivo dal socket */
int test2_callback(int socket, void* data);
/** Questo test usa una callback che chiude il socket */
int test3_callback(int socket, void* data);
/** Questo test usa una callback che invia (con un unico pacchetto tcp) più byte rispetto a quelli dichiarati nel Content-Length (32 invece di 20) */
int test4_callback(int socket, void* data);
/** Questo test usa una callback che invia (con un 2 pacchetto tcp, header e body) più byte rispetto a quelli dichiarati nel Content-Length (32 invece di 20) */
int test5_callback(int socket, void* data);
/** Questo test usa una callback che invia (con un unico pacchetto tcp) meno byte rispetto a quelli dichiarati nel Content-Length (32 invece di 50) */
int test6_callback(int socket, void* data);
/** Questo test usa una callback che invia (con un 2 pacchetto tcp, header e body) meno byte rispetto a quelli dichiarati nel Content-Length (32 invece di 50) */
int test7_callback(int socket, void* data);

#endif