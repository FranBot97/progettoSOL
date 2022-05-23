#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H
#include <stdio.h>
#include <request.h>
#include <stdbool.h>
#include <pthread.h>

/** Gestore della richiesta:
*
* Funzione eseguita dai singoli worker thread.
* Esamina il tipo di richiesta, chiama la rispettiva funzione sullo storage e invia il risultato al client.
* Scrive nel file di log.
* Controlla se è stato generato un errore fatale e nel caso termina il server.
* Comunica con il thread main per reintrodurre un client nel SET per l'operazione di select();
**/

void request_handler_function(request_t* request);

// Operazioni sullo storage //
/* Tutte le operazioni seguono circa lo stesso schema:
 * > Ricezione delle informazioni dal client
 * > Chiamata della rispettiva funzione di storage.c
 * > Invio responso/dati al client
*/

int server_openFile(request_t* request);

int server_readFile(request_t* request);

int server_readNFiles(request_t* request);

int server_writeFile(request_t* request);

int server_appendToFile(request_t* request);

int server_lockFile(request_t* request);

int server_unlockFile(request_t* request);

int server_closeFile(request_t* request);

int server_removeFile(request_t* request);

// Funzioni di supporto //

int server_addClientWaiting(request_t* request);

int server_removeClientWaiting(request_t* request);

int write_logfile(request_t* request, const char* OP, int IDCLIENT, unsigned int DELETED_BYTES, unsigned int ADDED_BYTES,
                  unsigned int SENT_BYTES, const char* OBJECT_FILE, const char* OUTCOME);

void fatal_logfile(request_t* request, int error);

#endif //REQUEST_HANDLER_H
