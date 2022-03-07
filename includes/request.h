//
// Created by linuxlite on 22/02/22.
//

#ifndef REQUEST_H
#define REQUEST_H
#include <pthread.h>
#include <sys/select.h>
#include <storage.h>

typedef struct request{
    int len;
    char *command;
    int client_fd;
   // pthread_mutex_t* storage_mutex;
    //pthread_mutex_t* files_mutex;
    storage_t* myStorage;
    int pipe_write;
} request_t;


#endif //REQUEST_H
