//
// Created by linuxlite on 21/02/22.
//
#include "../includes/storage.h"
#include "../includes/icl_hash.h"
#include <stdlib.h>
#include "../includes/list.h"
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include "../includes/util.h"
#include "../includes/request.h"
int compareInt(void* a, void* b){
    if(*(int*)a == *(int*)b)
        return 0;
    else
        return -1;
}
//TODO vedere come gestire errori lock/unlock fatali ?

/*
file_t* file_create(char* nome_file, unsigned long dimensione_file,  void* contenuto_file, int client_lock){

    file_t *newFile;
    newFile = (file_t*)malloc(sizeof(file_t));
    if (!newFile) return NULL;

    strcpy(newFile->filename, nome_file);
    newFile->size = dimensione_file;
    newFile->client_locker = client_lock;
    if(contenuto_file != NULL) {
        newFile->content = malloc(dimensione_file);
        memcpy(newFile->content, contenuto_file, dimensione_file);
    }else{
        newFile->content = NULL;
    }
    newFile->mutex = rwlock_init();

    return newFile;
}*/

void file_destroy(file_t *myfile){

    if(!myfile) return;

    if(myfile->filename)
        free(myfile->filename);

    if((myfile->content))
        free((myfile->content));

    if(myfile->who_opened)
        list_destroy(myfile->who_opened, free);

    if(myfile->mutex)
        rwlock_destroy(myfile->mutex);

    myfile->size = -1;
    myfile->client_locker = -1;

    if(myfile){
        free(myfile);
        myfile = NULL;
    }
}

//Sets errno malloc
storage_t* storage_create(int files_limit, unsigned long long memory_limit, int replace_mode){

    //Errore: "max_file" e "max_memoria" non possono essere <= 0
    if(files_limit <= 0 || memory_limit <= 0 )
        return NULL;

    storage_t* storage_return = (storage_t*)malloc(sizeof(storage_t));
    if(storage_return == NULL)
        return NULL;
    storage_return->files_limit = files_limit;
    storage_return->memory_limit = memory_limit;
    storage_return->files_number = 0;
    storage_return->occupied_memory = 0;
    storage_return->max_files_number = 0;
    storage_return->max_occupied_memory = 0;
    storage_return->times_replacement_algorithm = 0;
    storage_return->mutex = rwlock_init();
    if(storage_return->mutex == NULL)
        return NULL;
    storage_return->filenames_queue = list_create();
    if(storage_return->filenames_queue == NULL){
        rwlock_destroy(storage_return->mutex);
        return NULL;
    }
    storage_return->files = icl_hash_create((int)files_limit, hash_pjw, string_compare);
    if(storage_return->files == NULL){
        rwlock_destroy(storage_return->mutex);
        list_destroy(storage_return->filenames_queue, NULL);
        return NULL;
    }
    for(int i = 0; i < MAX_CONN; i++){
        storage_return->clients_waiting_lock[i] = -1;
    }

    return storage_return;
}

//TODO vedere quando fare la lock
void storage_destroy(storage_t* storage){

    if(!storage)
        return;

    //prima di eliminarlo stampa le statistiche
    storage_printStats(storage);

    if(storage->files != NULL) {
        icl_hash_destroy(storage->files, free, (void (*)(void *)) file_destroy);
        storage->files = NULL;
    }

    if(storage->filenames_queue != NULL) {
        list_destroy(storage->filenames_queue, free);
        storage->filenames_queue = NULL;
    }

    if(storage->mutex != NULL){
        rwlock_destroy(storage->mutex);
        storage->mutex = NULL;
    }

    if(storage) {
        free(storage);
        storage = NULL;
    }

}

//I file vuoti appena inseriti non causano nè subiscono espulsione
//poiché sono visti come file di peso pari a 0.
//L'espulsione avviene solo al momento della scrittura.
int storage_openFile(storage_t* storage, char* filename, int flags, int client){

    if(!storage || !filename ){
        errno = EINVAL;
        return -1;
    }

    if(flags & O_CREATE){
        //Con flag O_CREATE devo prima verificare se il file esiste già
        //Acquisisco la mutua esclusione in scrittura per eventualmente aggiungere il file
        if (rwlock_writerLock(storage->mutex) != 0) return FATAL;

        if (icl_hash_find(storage->files, (void*)filename) != NULL){
            if (rwlock_writerUnlock(storage->mutex) != 0) return FATAL;
            errno = EEXIST;
            return FAILURE;
        }

        //Se il file non esiste lo creo
        file_t* newFile = (file_t*) malloc(sizeof(file_t));
        if(newFile == NULL) return FATAL;
        newFile->who_opened = list_create();
        newFile->content = NULL;
        newFile->mutex = rwlock_init();
        newFile->size = 0;
        //Trasformo il numero client in stringa
        char client_str[MAX_STRLEN];
        snprintf(client_str, MAX_STRLEN, "%d", client);
        unsigned long client_strlen = strlen(client_str);
        char* client_mal = (char*)malloc( client_strlen*sizeof(char)+1);
        client_mal[client_strlen] = '\0';
        strcpy(client_mal,client_str);

        list_append(newFile->who_opened, client_mal);
        newFile->filename = (char*)malloc(strlen(filename)*sizeof(char)+1);
        strcpy(newFile->filename, filename);
        //Se è settato anche il flag di lock lo assegno al client
        if(flags & O_LOCK)
            newFile->client_locker = client;
        else
            newFile->client_locker = -1;

        //Lo aggiungo
        icl_hash_insert(storage->files,(void*)filename, (void*)newFile);
        if (rwlock_writerUnlock(storage->mutex) != 0) return FATAL;

        return SUCCESS;

    }

    //Altrimenti procedo senza creazione del file

    //Posso acquisire la mutua esclusione come lettore dato che non modifico la struttura
    if (rwlock_readerLock(storage->mutex) != 0) return FATAL;
    file_t* toOpen = NULL;
    //Se il file non esiste fallisco dato che non ho specificato il flag di creazione
    if ( (toOpen = icl_hash_find(storage->files, (void*)filename)) == NULL){
        if (rwlock_readerUnlock(storage->mutex) != 0) return FATAL;
        errno = ENOENT;
        return FAILURE;
    }
    if (rwlock_writerLock(toOpen->mutex) != 0) return FATAL;
    if (rwlock_readerUnlock(storage->mutex) != 0) return FATAL;

    if(flags & O_LOCK){ //TODO da cambiare deve agire come la lockfile!!
        //Se c'è il flag O_LOCK settato controllo se è possibile fare la lock del file
        if(toOpen->client_locker == -1 || toOpen->client_locker == client){
            toOpen->client_locker = client; //Imposto il proprietario
        }else{
            //Altrimenti fallisco
            if (rwlock_writerUnlock(toOpen->mutex) != 0) return FATAL;
            errno = EACCES;
            return FAILURE;
        }
    }
    char client_str[MAX_STRLEN];
    snprintf(client_str, MAX_STRLEN, "%d", client);
    //Aggiungo il client alla lista di chi ha aperto il file solo se non lo aveva già aperto
    if(list_get_elem(toOpen->who_opened, client_str, (void*)strcmp) == NULL){
        unsigned long client_strlen = strlen(client_str);
        char* client_mal = (char*)malloc( client_strlen*sizeof(char)+1);
        client_mal[client_strlen] = '\0';
        strcpy(client_mal,client_str);
        if (list_append(toOpen->who_opened, client_mal) == NULL)
            return FATAL;
    }
    if (rwlock_writerUnlock(toOpen->mutex) != 0) return FATAL;

    free(filename);
    return SUCCESS;
}

int storage_writeFile(storage_t* storage, const char* filename, size_t file_size, void* file_data, int client, list_t* list){
    return -1;
}

int storage_lockFile(storage_t* storage, char* filename, int client){

    if(!storage || !filename || client < 0){
        errno = EINVAL;
        return FAILURE;
    }
    file_t* toLock;

    //Acquisisco readerLock sullo storage
    if(rwlock_readerLock(storage->mutex) != 0) return FATAL;

    //il file non esiste
    if ( (toLock = icl_hash_find(storage->files, filename)) == NULL){
        if(rwlock_readerUnlock(storage->mutex) != 0) return FATAL;
        errno = ENOENT;
        return FAILURE;
    }

    //il file esiste, prendo la lock in scrittura sul file e rilascio lo storage
    if(rwlock_writerLock(toLock->mutex) != 0) return FATAL;
    if(rwlock_readerUnlock(storage->mutex) != 0) return FATAL;

    //controllo se il client ha aperto il file
    char client_str[MAX_STRLEN];
    snprintf(client_str, MAX_STRLEN, "%d", client);
    if(list_get_elem(toLock->who_opened, client_str, (void*)strcmp) == NULL){
        if(rwlock_writerUnlock(toLock->mutex) != 0) return FATAL;
        errno = EPERM;
        return FAILURE;
    }

    //il locker è già il client richiedente o è libero
    if(toLock->client_locker == client || toLock->client_locker == -1){
        toLock->client_locker = client;
        if(rwlock_writerUnlock(toLock->mutex) != 0) return FATAL;
        return SUCCESS;
    }

    //Un altro client è il locker
    //Non posso completare la richiesta adesso, libero il thread da questo task
    if(rwlock_writerUnlock(toLock->mutex) != 0) return FATAL;

    errno = EACCES;
    return FAILURE;
}

int storage_unlockFile(storage_t* storage, const char* filename, int client){

    if(!storage || !filename || client < 0){
        errno = EINVAL;
        return FAILURE;
    }
    file_t* toLock;

    //Acquisisco readerLock sullo storage
    if(rwlock_readerLock(storage->mutex) != 0) return FATAL;

    //il file non esiste
    if ( (toLock = icl_hash_find(storage->files, (void*)filename)) == NULL){
        if(rwlock_readerUnlock(storage->mutex) != 0) return FATAL;
        errno = ENOENT;
        return FAILURE;
    }

    //il file esiste, prendo la lock in scrittura sul file e rilascio lo storage
    if(rwlock_writerLock(toLock->mutex) != 0) return FATAL;
    if(rwlock_readerUnlock(storage->mutex) != 0) return FATAL;

    //controllo se il client ha aperto il file
    char client_str[MAX_STRLEN];
    snprintf(client_str, MAX_STRLEN, "%d", client);
    if(list_get_elem(toLock->who_opened, client_str, (void*)strcmp) == NULL){
        if(rwlock_writerUnlock(toLock->mutex) != 0) return FATAL;
        errno = EPERM;
        return FAILURE;
    }

    //il locker era il client richiedente OK
    if(toLock->client_locker == client){
        toLock->client_locker = -1;
        if(rwlock_writerUnlock(toLock->mutex) != 0) return FATAL;
        return SUCCESS;
    }

    //Nessuno è il locker
    if(toLock->client_locker == -1){
        if (rwlock_writerUnlock(toLock->mutex) != 0) return FATAL;
        errno = EALREADY;
        return FAILURE;
    }

    //Un altro client è il locker
    if(toLock->client_locker != -1) {
        if (rwlock_writerUnlock(toLock->mutex) != 0) return FATAL;
        errno = EACCES;
        return FAILURE;
    }

    return FAILURE;
}

int storage_closeFile(storage_t* storage, char* filename, int client){

    if(!storage || !filename){
        errno = EINVAL;
        return -1;
    }

    if(rwlock_readerLock(storage->mutex) != 0 ) return FATAL;
    file_t* toClose;
    //Se il file che voglio chiudere non esiste
    if ( (toClose = icl_hash_find(storage->files, filename)) == NULL){
        if(rwlock_readerUnlock(storage->mutex) != 0 ) return FATAL;
        errno = ENOENT;
        return FAILURE;
    }
    //Prendo writelock sul file e rilascio lo storage
    if(rwlock_writerLock(toClose->mutex) != 0) return FATAL;
    if(rwlock_readerUnlock(storage->mutex) != 0) return FATAL;

    //Controllo che il client sia tra quelli che hanno aperto il file
    char client_str[MAX_STRLEN];
    snprintf(client_str, MAX_STRLEN, "%d", client);
    //Se non c'è
    elem_t* removedElem;
    if((removedElem = list_remove_elem(toClose->who_opened, client_str, (void*)strcmp)) == NULL){
        if(rwlock_writerUnlock(toClose->mutex) != 0) return FATAL;
        errno = EALREADY; //operazione già effettuata
        return FAILURE;
    }else{ //Altrimenti ho correttamente rimosso il client dalla lista
        //libero le risorse
        free(removedElem->content);
        free(removedElem);
        if(rwlock_writerUnlock(toClose->mutex) != 0) return FATAL;
        return SUCCESS;
    }

}

int storage_removeFile(storage_t* storage, char* filename, int client){

    if(!storage || !filename){
        errno = EINVAL;
        return -1;
    }

    //Dato che vado a modificare la struttura dello storage entro come scrittore
    //in modo da essere l'unico ad operare
    if(rwlock_writerLock(storage->mutex) != 0) return FATAL;
    file_t* toRemove;
    //Se il file non esiste
    if((toRemove = icl_hash_find(storage->files, filename)) == NULL){
        if(rwlock_writerUnlock(storage->mutex) != 0) return FATAL;
        errno = ENOENT;
        return FAILURE;
    }
    //Se il file esiste prendo la writerLock prima
    if(rwlock_writerLock(toRemove->mutex) != 0) return FATAL;
    //Controllo se il client ha fatto la lock
    if(toRemove->client_locker == client){
        //qui posso procedere

        if(toRemove->size > 0){
            //Se il file aveva effettivamente un contenuto allora modifico i numero di file e la memoria occupata
            storage->occupied_memory -= toRemove->size;
            storage->files_number--;
            elem_t* toRemove_elem = list_remove_elem(storage->filenames_queue, filename, (void*)strcmp);
            if(!toRemove_elem) return FATAL;
            free(toRemove_elem->content);
            free(toRemove);
        }
        //Elimino dalla tabella dei file
        if (icl_hash_delete(storage->files, filename, free, (void*)file_destroy) != 0) return FATAL;

        //Rilascio la lock sullo storage solo alla fine
        if(rwlock_writerUnlock(storage->mutex) != 0) return FATAL;

        return SUCCESS;

    }else{//Il client non è il locker
        if(rwlock_writerUnlock(toRemove->mutex) != 0) return FATAL;
        if(rwlock_writerUnlock(storage->mutex) != 0) return FATAL;
        errno = EACCES;
        return FAILURE;
    }

}



int storage_addClientWaiting(storage_t* storage, int client){

    if(!storage || client < 0){
        errno = EINVAL;
        return FAILURE;
    }

    if (rwlock_writerLock(storage->mutex) != 0) return FATAL;
    int i;
    for(i = 0 ; i < MAX_CONN; i++){
        if(storage->clients_waiting_lock[i] != -1)
            continue;
        else{
            storage->clients_waiting_lock[i] = client;
            break;
        }
    }
    if (rwlock_writerUnlock(storage->mutex) != 0) return FATAL;

    if(i == MAX_CONN) //significa che non ho trovato spazio per aggiungere il client
        return FAILURE;

    return SUCCESS;
}

int storage_removeClientWaiting(storage_t* storage){

    if(!storage){
        errno = EINVAL;
        return FAILURE;
    }

    if (rwlock_writerLock(storage->mutex) != 0) return FATAL;
    for(int i = 0; i < MAX_CONN; i++){
        if(storage->clients_waiting_lock[i] != -1) {
            int result = storage->clients_waiting_lock[i];
            storage->clients_waiting_lock[i] = -1;
            if (rwlock_writerUnlock(storage->mutex) != 0) return FATAL;
            return result;
        }
    }
    if (rwlock_writerUnlock(storage->mutex) != 0) return FATAL;

    return FAILURE;
}

int storage_printStats(storage_t* storage){

    if(!storage)
        return -1;

    printf("\n=========== STATISTICHE FILE STORAGE ===============\n");
    printf("Numero massimo di file raggiunto: %d\n", storage->max_files_number);
    printf("Dimensione massima della memoria occupata %llu MB\n", (storage->max_occupied_memory)/1024/1024 );
    printf("Algoritmo di rimpiazzamento eseguito %d volte\n", storage->times_replacement_algorithm);
    printf("----- FILES ------\n");
    list_printAsString(storage->filenames_queue);
    printf("-------------------\n");
    printf("\n====================================================\n");

    return 0;

}