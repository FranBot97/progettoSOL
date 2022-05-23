#ifndef REQUEST_H
#define REQUEST_H
#include <pthread.h>
#include <sys/select.h>
#include <storage.h>

/**  Struct che rappresenta una generica richiesta di un client.
 *
 *  @param opcode - codice identificativo del tipo di richiesta
 *  @param client_fd - identificativo del client
 *  @param storage - puntatore al File Storage su cui eseguire l'operazione
 *  @param pipe_write - pipe per comunicare con il main thread
 *  @param logfile - puntatore al file di log
 *  @param logfile_mutex - mutex per accedere in mutua esclusione al file
 **/
typedef struct request{
    int opcode;
    long client_fd;
    storage_t* storage;
    int pipe_write;
    //logfile + mutex
    FILE *logfile;
    pthread_mutex_t* logfile_mutex;
} request_t;

/* Operazioni possibili con i rispettivi codici */
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

/* Flags per l'operazione di openFile */
enum flags_   {
    O_CREATE = 1 << 0,
    O_LOCK  = 1 << 1
};

/* Codici degli errori */
enum errors_{
    SUCCESS = 0,
    FAILURE = -1,
    FATAL = -2,
};


#endif //REQUEST_H
