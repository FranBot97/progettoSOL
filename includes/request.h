//
// Created by linuxlite on 22/02/22.
//

#ifndef REQUEST_H
#define REQUEST_H
#include <pthread.h>
#include <sys/select.h>
#include <storage.h>

typedef struct request{
    int opcode;
    long client_fd;
    storage_t* storage;
    int pipe_write;
    //logfile + mutex
    FILE *logfile;
    pthread_mutex_t* logfile_mutex;
} request_t;


enum opcode_{
    OPEN = 1,
    WRITE = 2,
    READ = 3,
    READN = 4,
    LOCK = 5,
    UNLOCK = 6,
    REMOVE = 7,
    CLOSE = 8,
    APPEND = 9,
    NONE = 0,
    SHUTDOWN = -1
};

enum flags_   {
    O_CREATE = 1 << 0,
    O_LOCK  = 1 << 1
};

enum errors_{
    SUCCESS = 0,
    FAILURE = -1,
    FATAL = -2,
};


#endif //REQUEST_H
