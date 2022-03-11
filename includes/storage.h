//
// Created by linuxlite on 21/02/22.
//
#include "icl_hash.h" //TODO mettere <>
#include <list.h>
#include <pthread.h>
#ifndef NOVEMBREPROG_STORAGE_H
#define NOVEMBREPROG_STORAGE_H


typedef struct storage_{

    int max_file;
    int max_memoria;

    int numero_file; //numero di file nello storage
    int memoria_occupata;
    icl_hash_t* files;
    list_t* coda_file; //per eventuale  FIFO

    pthread_mutex_t storage_mutex;

}storage_t;

storage_t* storage_create(int max_file, int max_memoria, int replace_mode);

void storage_destroy(storage_t* storage);

int storage_addfile(storage_t* storage, file_t* file, char* filename, list_t** expelledFiles);

file_t* storage_findFile(storage_t* storage, const char* filename);

int storage_lockFile(storage_t* storage, const char* filename, int client_fd);

int storage_unlockFile(storage_t* storage, const char* filename, int client_fd);

#endif //NOVEMBREPROG_STORAGE_H