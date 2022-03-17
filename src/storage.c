//
// Created by linuxlite on 21/02/22.
//
#include <storage.h>
#include <icl_hash.h>
#include <stdlib.h>
#include <file.h>
#include <list.h>
#include <string.h>
#include <errno.h>

//TODO PROSSIMA COSA DA FARE: GESTIRE ESPULSIONE FILE E CONTROLLARE PERCHE' NON VA

storage_t* storage_create(int max_file, unsigned long max_memoria, int replace_mode){

    //Errore: "max_file" e "max_memoria" non possono essere <= 0
    if(max_memoria <= 0 || max_file <= 0 )
        return NULL;
    storage_t* storage_return = (storage_t*)malloc(sizeof(storage_t));

    storage_return->max_file = max_file;
    storage_return->max_memoria = max_memoria;
    storage_return->files = icl_hash_create((int)max_file, hash_pjw, string_compare);
    storage_return->coda_file = list_create(); //todo in base al replace_mode
    storage_return->numero_file = 0;
    storage_return->memoria_occupata = 0;
    pthread_mutex_init(&(storage_return->storage_mutex), NULL);

    return storage_return;
}

void storage_free_key(void* key){
    printf("LIBERO!!!!!!!!\n");
    free(key);
    key = NULL;
}

void storage_destroy(storage_t* storage){

    if(!storage)
        return;

    if(storage->files != NULL) {
        icl_hash_destroy(storage->files, storage_free_key, (void (*)(void *)) file_clean);
        storage->files = NULL;
    }

    if(storage->coda_file != NULL) {
        list_destroy(storage->coda_file, NULL);
        storage->coda_file = NULL;
    }

    if(storage) {
        free(storage);
        storage = NULL;
    }

}

int storage_removeFile(storage_t* storage, char* filename, int filesize){

    if(!storage || !filename)
        return -1;

    if(icl_hash_delete(storage->files, filename, storage_free_key, (void (*)(void *)) file_clean) == 0) {
        storage->memoria_occupata-=filesize;
        storage->numero_file--;
        return 0;
    }
    else
        return -1;

}

//restituisce il file_t espulso
int storage_espelliFile(storage_t* storage, size_t necess_memory, list_t** expelledFiles){

    if(necess_memory > storage->max_memoria){
        return -1;
    }

    if(storage->max_memoria - storage->memoria_occupata < necess_memory || storage->numero_file == storage->max_file)
        *expelledFiles = list_create();

    if(!expelledFiles)
        return -1;

    while(storage->max_memoria - storage->memoria_occupata < necess_memory || storage->numero_file == storage->max_file){
        char filename[MAX_FILENAME];
        elem_t* toDestroy = get_tail(storage->coda_file);
        strcpy(filename,  (char*)toDestroy->content); //problema qui, mi dice che toDestroy->content non è allocato
        free(toDestroy);

        file_t* victim = storage_findFile(storage, filename);
        if(!victim)
            continue;
        else {
            file_t* toSend = file_create(victim->nome_file, victim->dimensione_file, victim->contenuto_file, victim->client_lock);
            if(!toSend)
                return -1;

            if( storage_removeFile(storage, filename, victim->dimensione_file) != 0)
                return -1;

            add_head_element(*expelledFiles, toSend);
//TODO DEALLOCARE I FILE CHE POI VENGONO ESPULSI ????????

        }
    }
    return 0;

}

int storage_addfile(storage_t* storage, file_t* file, char* filename, list_t** expelledFiles){

    if(!file || !filename || !storage)
        return -1;

    //Se è stato raggiunto il numero massimo di file o se la memoria è piena devo espellere un file
    if(storage->numero_file == storage->max_file || (storage->memoria_occupata + file->dimensione_file) > storage->max_memoria){
        //ESPULSIONE FILE
        storage_espelliFile(storage, file->dimensione_file, *&expelledFiles);
        printf("ESPULSIONE\n");
    }

    if( icl_hash_insert(storage->files, (void*)filename, file) == NULL)
        return -1;

    else {
        storage->numero_file++;
        storage->memoria_occupata+= file->dimensione_file;
    }

    if(add_head_element(storage->coda_file, (void*)filename) != 0)
        return -2;

    return 0;

}

file_t* storage_findFile(storage_t* storage, const char* filename){

    if(!storage || !filename)
        return NULL;

    file_t* found = icl_hash_find(storage->files, (void*)filename);

    if(!found)
        return NULL;

    return found;
}

int storage_unlockFile(storage_t* storage, const char* filename, int client_fd){

    if(!storage || !filename)
        return -1;

    file_t* modify = icl_hash_find(storage->files, (void*)filename);

    if(!modify)
        return -1;

    //File non lockato
    if(modify->client_lock == -1)
        return 2;

    if(modify->client_lock != client_fd)
        return 1;

    if(modify->client_lock == client_fd)
        modify->client_lock = -1;

    return 0;

}

int storage_lockFile(storage_t* storage, const char* filename, int client_fd){

    if(!storage || !filename)
        return -1;

    file_t* modify = icl_hash_find(storage->files, (void*)filename);

    if(!modify)
        return -1;

    if(modify->client_lock != -1 && modify->client_lock != client_fd)
        return 1;
    else
        modify->client_lock = client_fd;

    return 0;

}

int storage_writeFileContent(storage_t* storage, file_t* file, const char* filename, void* file_content, size_t len, int flags, list_t** expelledFiles){

    if(!storage || !file_content || len > storage->max_memoria) {
        errno = EINVAL;
        return -1;
    }

    if(file == NULL){
        file = storage_findFile(storage, filename);
        if(file == NULL)
            return -1;
    }

    //Controllo e salvo eventuali file da espellere
    if(storage->max_file != 1) //Se c'è un solo file non si espelle perché è il file su cui devo scrivere
        storage_espelliFile(storage, len, *&expelledFiles);

    if(flags & O_REPLACE) { //Scrivi da zero

        //Se nel file c'era qualcosa allora prima rimuovo tutto
        if(file->dimensione_file != 0) {
            storage->memoria_occupata -= file->dimensione_file;

            if(file->contenuto_file)
                free(file->contenuto_file);

            file->dimensione_file = 0;
            file->contenuto_file = NULL;
            }

        file->contenuto_file = malloc(len);
        //Poi scrivo da zero
        memcpy(file->contenuto_file, file_content, len);
        file->contenuto_file = file_content;
        file->dimensione_file = len;
        storage->memoria_occupata += file->dimensione_file;

    }else{
        //Scrivi in append
        if ( (file->contenuto_file = realloc(file->contenuto_file, file->dimensione_file + len)) == NULL){
            perror("Malloc");
            exit(EXIT_FAILURE);
        }
        memcpy(file->contenuto_file + file->dimensione_file, file_content, len);
        file->dimensione_file += len;
        storage->memoria_occupata += len;
    }
    return 0;
}