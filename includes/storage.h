//
// Created by linuxlite on 21/02/22.
//
#include <icl_hash.h>
#include <list.h>
#include <util.h>
#include <pthread.h>
#include <read_write_lock.h>
#ifndef STORAGE_H
#define STORAGE_H

typedef struct file_{
    char* filename;     //nome del file
    unsigned int size;  //dimensione in bytes
    void* content;      //contenuto generico
    int client_locker;  //client che ha fatto la lock
    list_t* who_opened; //lista di client che hanno il file attualmente aperto
    rwlock_t* mutex;     //read & write lock per mutua esclusione
}file_t;

typedef struct storage_{

    int files_limit;
    unsigned long long memory_limit; //in bytes

    int files_number; //numero di file nello storage
    unsigned long long occupied_memory; //in bytes
    icl_hash_t* files;
    list_t* filenames_queue; //lista dei nomi dei file in ordine di scrittura
    rwlock_t* mutex;
    int clients_waiting_lock[MAX_CONN];

    //Statistiche
    int max_files_number; //massimo numero di file raggiunto nello storage
    unsigned long long max_occupied_memory; //massima dimensione della memoria raggiunta in bytes
    int times_replacement_algorithm; //numero di volte che è stato eseguito l'algoritmo di rimpiazzamento

}storage_t;


storage_t* storage_create(int max_file, unsigned long long max_memoria, int replace_mode);

int storage_printStats(storage_t *storage);

void storage_destroy(storage_t* storage);

void file_destroy(file_t *myfile);

//Operazioni sui file//

int storage_openFile(storage_t* storage, char* filename, int flags, int client);

int storage_closeFile(storage_t* storage, char* filename, int client);

int storage_removeFile(storage_t* storage, char* filename, int client);

int storage_lockFile(storage_t* storage, char* filename, int client);

int storage_unlockFile(storage_t* storage, const char* filename, int client);

int storage_writeFile(storage_t* storage, const char* filename, size_t file_size, void* file_content, int client, list_t** list);
//Restituisce numero bytes letti
long long storage_readFile(storage_t* storage, char* filename, int client, void** buf);

// Funzioni di supporto //
int storage_addClientWaiting(storage_t* storage, int client);

int storage_removeClientWaiting(storage_t* storage);

#endif //STORAGE_H
