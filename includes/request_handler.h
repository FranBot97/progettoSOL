//
// Created by linuxlite on 22/02/22.
//

#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H
#include <stdio.h>
#include <request.h>
#include <stdbool.h>
#include <pthread.h>

void request_handler_function(request_t* request);

// Operazioni sullo storage //

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

#endif //REQUEST_HANDLER_H
