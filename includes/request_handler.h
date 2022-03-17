//
// Created by linuxlite on 22/02/22.
//

#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H
#include <stdio.h>
#include <request.h>
#include <pthread.h>

void request_handler_function(request_t* request);

int manage_storage(pthread_mutex_t* file_storage, char* action);

int writeClient(char* msg, int client_fd);

int readClientSingle(void* content, int client_fd, size_t size);

int readClient(void** content, int client_fd, const char* contentType);

int openFile(request_t* request, char* pathname);

int writeFile(request_t* request, char* filename);

int unlockFile(request_t* request, char* filename);

#endif //REQUEST_HANDLER_H
