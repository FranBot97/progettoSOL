#include <storage.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <request.h>

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
    storage_return->replace_mode = replace_mode;
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

void storage_destroy(storage_t* storage){

    if(!storage)
        return;

    if (rwlock_writerLock(storage->mutex) != 0) return;

    //prima di eliminarlo stampa le statistiche
    storage_printStats(storage);

    // Chiudo tutte le connessioni dei client che aspettavano una unlock
    for(int i=0; i < MAX_CONN; i++){
        int client = storage->clients_waiting_lock[i];
        if(client != -1) close(client);
    }

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

// Operazioni sui file //

//I file vuoti appena inseriti non causano nè subiscono espulsione
//poiché sono visti come file di peso pari a 0.
//L'espulsione avviene solo al momento della scrittura.
int storage_openFile(storage_t* storage, char* filename, int flags, int client){

    if(!storage || !filename ){
        errno = EINVAL;
        return FAILURE;
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
        if(client_mal == NULL) return FATAL;
        client_mal[client_strlen] = '\0';
        strcpy(client_mal,client_str);

        list_append(newFile->who_opened, client_mal);
        newFile->filename = (char*)malloc(strlen(filename)*sizeof(char)+1);
        if(newFile->filename == NULL) return FATAL;
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

    if(flags & O_LOCK){
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

long long storage_readFile(storage_t* storage, char* filename, int client, void** buf){

    if(!storage || !filename || !buf){
        errno = EINVAL;
        return 0;
    }

    //Prendo la readerLock sullo storage
    if(rwlock_readerLock(storage->mutex) != 0) return FATAL;
    //Cerco se è presente il file
    file_t* toRead;
    if( (toRead = icl_hash_find(storage->files, filename)) == NULL){
        if(rwlock_readerUnlock(storage->mutex) != 0) return FATAL;
        errno = ENOENT;
        return 0;
    }
    //Prendo la readerLock sul file e rilascio quella sullo storage
    if(rwlock_readerLock(toRead->mutex) != 0) return FATAL;
    if(rwlock_readerUnlock(storage->mutex) != 0) return FATAL;
    //Controllo che il client abbia aperto il file
    char client_str[MAX_STRLEN];
    snprintf(client_str, MAX_STRLEN, "%d", client);
    if(list_get_elem(toRead->who_opened, client_str, (void*) strcmp ) == NULL){
        if(rwlock_readerUnlock(toRead->mutex) != 0) return FATAL;
        errno = EACCES;
        return FAILURE;
    }
    //Il client ha aperto il file
    //Controllo che il file sia unlocked oppure locked dal client che ha chiesto la read
    if(toRead->client_locker == client || toRead->client_locker == -1){

        //Posso leggere il file solo se il contenuto è > 0
        if(toRead->size == 0){
            if(rwlock_readerUnlock(toRead->mutex) != 0) return FATAL;
            errno = ENODATA;
            return FAILURE;
        }

        //Memorizzo il contenuto del file nel buffer e restituisco la quantità di bytes
        *buf = malloc(toRead->size);
        if(!*buf) return FATAL;
        memcpy(*buf, toRead->content, toRead->size);
        unsigned long size = toRead->size;
        if(rwlock_readerUnlock(toRead->mutex) != 0) return FATAL;
        return size;
    }else{
        //il file appartiene ad un altro client, non si può leggere
        if(rwlock_readerUnlock(toRead->mutex) != 0) return FATAL;
        errno = EPERM;
        return FAILURE;
    }
}

//Legge solo i file con contenuto e non locked, i file vuoti non vengono considerati
int storage_readNFiles(storage_t* storage, int client, int N, list_t** files_to_send){

    if(!storage || !files_to_send){
        errno = EINVAL;
        return FAILURE;
    }
    errno = 0;
    int count_files = 0;
    *files_to_send = NULL;
    //Dato che devo leggere tutti i file entro in lettura
    if(rwlock_readerLock(storage->mutex) != 0) return FATAL;
    if(N > storage->files_number || N<=0) N = storage->files_number;
    //Per ogni nome di file nella lista dello storage vado a prendere il corrispondente
    //file dalla tabella e lo aggiungo alla lista files_to_send
    elem_t* elem = list_get_head(storage->filenames_queue);
    if(!elem) goto cleanup; //non ci sono file con contenuto nello storage
    while(count_files < N){
        if(*files_to_send == NULL) *files_to_send = list_create();
        if(!files_to_send) return FATAL;
        //Cerco il file nello storage
        file_t* file_to_copy = icl_hash_find(storage->files, elem->content);
        file_t* file = NULL;
        if(file_to_copy){
            //se il file esiste ne prendo la readerLock
            if(rwlock_readerLock(file_to_copy->mutex) != 0) return FATAL;
            //Se il file è locked dal client che chiede la lettura o è libero allora posso leggerlo
            if(file_to_copy->client_locker == -1 || file_to_copy->client_locker == client){
                //lo leggo
                // Creo il file di copia
                file = (file_t*)malloc(sizeof(file_t));
                if(file == NULL) return FATAL;
                memset(file, 0, sizeof(file_t));
                if(!file) return FATAL;
                //oscuro i dati non rilevanti
                file->mutex = NULL; file->client_locker = -1; file->who_opened = NULL;
                //Alloco lo spazio
                file->size = file_to_copy->size;
                file->content = malloc(file_to_copy->size);
                if(!file->content) return FATAL;
                file->filename = malloc(strlen(file_to_copy->filename)*sizeof(char)+1);
                if(!file->filename) return FATAL;
                //Copio il contenuto
                memcpy(file->content, file_to_copy->content, file_to_copy->size);
                memcpy(file->filename, file_to_copy->filename, strlen(file_to_copy->filename)+1);
                //Aggiungo il file alla lista
                if( list_append(*files_to_send, file) == NULL) {
                    int errno_copy = errno;
                    if(rwlock_readerUnlock(file_to_copy->mutex) != 0) return FATAL;
                    if(rwlock_readerUnlock(storage->mutex) != 0) return FATAL;
                    //problema con la lista, fallisco per il client
                    file_destroy(file);
                    list_destroy(*files_to_send, (void (*)(void *)) file_destroy);
                    errno = errno_copy;
                    return FAILURE;
                }
                count_files++;
            }
            //Rilascio readerLock
            if(rwlock_readerUnlock(file_to_copy->mutex) != 0) return FATAL;
        }else{
            //il file non esiste nello storage ma esiste nella lista dei file,
            //inconsistenza non grave, segnalo ma vado avanti
            fprintf(stderr, "Il file %s compare nella lista ma non nello storage\n", (char*)elem->content);
        }
        elem = list_getNext(storage->filenames_queue, elem);
        if(!elem) break; //Se è NULL allora ho esaminato tutti i file del server e esco prima
    }
    cleanup:
    if(rwlock_readerUnlock(storage->mutex) != 0) return FATAL;
    return count_files;

}

int storage_writeFile(storage_t* storage, const char* filename, size_t file_size, void* file_data, int client, list_t** list){

    if(!storage || !filename || !file_data || file_size <= 0 || !list){
        errno = EINVAL;
        return FAILURE;
    }

    //Devo controllare che il file esista e che sia aperto e locked dal client
    //Tutto in modalità scrittore perché poi potrei modificare la struttura dati
    if(rwlock_writerLock(storage->mutex) != 0) return FATAL;
    file_t* toWrite = NULL;
    if ( (toWrite = icl_hash_find(storage->files, (void*)filename)) == NULL){
        //Se il file non esiste
        if(rwlock_writerUnlock(storage->mutex) != 0) return FATAL;
        errno = ENOENT;
        return FAILURE;
    }
    //Se esiste prendo la writerLock
    if(rwlock_writerLock(toWrite->mutex) != 0) return FATAL;
    //Controllo che il file non sia già stato scritto in passato
    if(toWrite->size > 0){
        if(rwlock_writerUnlock(toWrite->mutex) != 0) return FATAL;
        if(rwlock_writerUnlock(storage->mutex) != 0) return FATAL;
        errno = EALREADY;
        return FAILURE;
    }
    //Controllo che la dimensione da scrivere non sia superiore al limite della memoria
    if(file_size > storage->memory_limit){
        if(rwlock_writerUnlock(toWrite->mutex) != 0) return FATAL;
        if(rwlock_writerUnlock(storage->mutex) != 0) return FATAL;
        errno = EFBIG;
        return FAILURE;
    }
    //Controllo che sia stato aperto
    char client_str[MAX_STRLEN];
    snprintf(client_str, MAX_STRLEN, "%d", client);
    if( (list_get_elem(toWrite->who_opened, client_str, (void*)strcmp)) == NULL){
        //il client non ha aperto il file
        if(rwlock_writerUnlock(toWrite->mutex) != 0) return FATAL;
        if(rwlock_writerUnlock(storage->mutex) != 0) return FATAL;
        errno = EACCES;
        return FAILURE;
    }
    //il client ha aperto il file, controllo sia anche il locker
    if(toWrite->client_locker != client){
        //il client non è il locker
        if(rwlock_writerUnlock(toWrite->mutex) != 0) return FATAL;
        if(rwlock_writerUnlock(storage->mutex) != 0) return FATAL;
        errno = EACCES;
        return FAILURE;
    }
    do {
    //Rimpiazzamento file
    //Se aumentando di 1 il numero di file e aggiungendo la dimensione del file rimango nei limiti
    //allora non devo fare rimpiazzamenti
    if( (storage->files_number+1 <= storage->files_limit) && (storage->occupied_memory+file_size <= storage->memory_limit) )
        goto write;

    //Altrimenti significa che non c'è abbastanza spazio e devo liberare dei file
    //prendo il nome del primo file da eliminare (FIFO)
    elem_t *elem = list_remove_head(storage->filenames_queue);
    if(elem == NULL) //Se sono arrivato qua e non ho file da liberare significa che c'è un errore di inconsistenza dello storage
        {   errno = ENOTRECOVERABLE;
            return FATAL;}

    char *exp_file_name = elem->content;
    //Vado a cercarlo nello storage
    file_t *toExp = icl_hash_find(storage->files, exp_file_name);
    if (toExp == NULL){ //se il file non c'è più lo ignoro
        continue;
    }

    //Ora ho il file da eliminare, prendo la writerLock anche su di lui
    if(rwlock_writerLock(toExp->mutex) != 0) return FATAL;

    //Creo e aggiungo una copia del file alla lista dei file espulsi
    if(*list == NULL) *list = list_create();
    if(*list == NULL) return FATAL;
    file_t* toExp_copy = (file_t*)malloc(sizeof(file_t));
    if(toExp_copy == NULL) return FATAL;
    toExp_copy->who_opened = NULL; //oscuro i dati non rilevanti
    toExp_copy->mutex = NULL; //oscuro i dati non rilevanti
    //Mi servono nome, dimensione e contenuto
    //nome:
    toExp_copy->filename = malloc(strlen(toExp->filename)*sizeof(char)+1);
    if(toExp_copy->filename == NULL) return FATAL;
    toExp_copy->filename[strlen(toExp->filename)] = '\0';
    strcpy(toExp_copy->filename, toExp->filename);
    //dimensione:
    toExp_copy->size = toExp->size;
    //contenuto:
    toExp_copy->content = malloc(toExp_copy->size);
    if(toExp_copy->content == NULL) return FATAL;
    memcpy(toExp_copy->content, toExp->content, toExp_copy->size);

    //Aggiungo alla lista
    list_append(*list, toExp_copy);

    //Modifico lo storage per l'eliminazione
    storage->occupied_memory -= toExp->size;
    storage->files_number--;
    storage->times_replacement_algorithm++;
    //Lo elimino dallo storage
    if (icl_hash_delete(storage->files, toExp->filename, free, (void*)file_destroy) != 0) return FATAL;
    if(exp_file_name) free(exp_file_name);
    if(elem) free(elem);
    }while(true);

    write:
    //Qui effettuo la scrittura del file
    toWrite->content = file_data;
    toWrite->size = file_size;
    //Modifico lo storage:
    //aggiungo il nome del file alla coda
    char* filename_copy = malloc(strlen(filename)*sizeof(char)+1);
    if(filename_copy == NULL) return FATAL;
    filename_copy[strlen(filename)] = '\0';
    strcpy(filename_copy, filename);
    if (list_append(storage->filenames_queue, filename_copy) == NULL)
        return FATAL;
    //modifico variabili dello storage
    storage->occupied_memory += file_size;
    storage->files_number++;
    if(storage->occupied_memory > storage->max_occupied_memory)
        storage->max_occupied_memory = storage->occupied_memory;
    if(storage->files_number > storage->max_files_number)
        storage->max_files_number = storage->files_number;

    if(rwlock_writerUnlock(toWrite->mutex) != 0) return FATAL;
    if(rwlock_writerUnlock(storage->mutex) != 0) return FATAL;

    return SUCCESS;
}

int storage_appendToFile(storage_t* storage, char* filename, size_t size, void* data, int client, list_t** list){

    if(!storage || !filename || size <= 0 || !data || !list){
        errno = EINVAL;
        return FAILURE;
    }

    if(rwlock_writerLock(storage->mutex) != 0) return FATAL;
    //Controllo che la dimensione del contenuto non sia eccessivamente grande
    if(size > storage->memory_limit){
        if(rwlock_writerUnlock(storage->mutex) != 0) return FATAL;
        errno = EFBIG;
        return FAILURE;
    }
    //Cerco il file
    file_t* toAppend;
    if ( (toAppend = icl_hash_find(storage->files, filename)) == NULL){
        if(rwlock_writerUnlock(storage->mutex) != 0) return FATAL;
        errno = ENOENT;
        return FAILURE;
    }
    if (rwlock_writerLock(toAppend->mutex) != 0) return FATAL;
    //il file è presente, controllo sia stato aperto dal client
    char client_str[MAX_STRLEN];
    snprintf(client_str, MAX_STRLEN, "%d", client);
    if(list_get_elem(toAppend->who_opened, client_str, (int (*)(void *, void *)) strcmp) == NULL){
        //il client non può accedere al file
        if(rwlock_writerUnlock(toAppend->mutex) != 0) return FATAL;
        if(rwlock_writerUnlock(storage->mutex) != 0) return FATAL;
        errno = EACCES;
        return FAILURE;
    }
    //controllo sia libero o locked dal client che ha fatto la richiesta
    if(toAppend->client_locker == -1 || toAppend->client_locker == client){
        //Posso procedere
        do {
            //Rimpiazzamento file
            //Se a aggiungendo la dimensione del file rimango nei limiti
            //allora non devo fare rimpiazzamenti
            if( storage->occupied_memory+size <= storage->memory_limit)
                goto append;

            //Altrimenti significa che non c'è abbastanza spazio e devo liberare dei file
            //prendo il nome del primo file da eliminare (FIFO)
            //Vado a rimuovere dalla lista il primo elemento diverso dal file che devo scrivere
            elem_t *elem = list_remove_elem(storage->filenames_queue, toAppend->filename,(int (*)(void *, void *))(string_compare));
            if(elem == NULL){
                //Se sono arrivato qua e non ho file da liberare significa che c'è un errore di inconsistenza dello storage
                errno = ENOTRECOVERABLE;
                return FATAL;
            }
            char *exp_file_name = elem->content;
            //Vado a cercarlo nello storage
            file_t *toExp = icl_hash_find(storage->files, exp_file_name);
            if (toExp == NULL){ //se il file non c'è più lo ignoro
                continue;
            }

            //Ora ho il file da eliminare, prendo la writerLock anche su di lui
            if(rwlock_writerLock(toExp->mutex) != 0) return FATAL;

            //Creo e aggiungo una copia del file alla lista dei file espulsi
            if(*list == NULL) *list = list_create();
            if(*list == NULL) return FATAL;
            file_t* toExp_copy = (file_t*)malloc(sizeof(file_t));
            if(toExp_copy == NULL) return FATAL;
            toExp_copy->who_opened = NULL; //oscuro i dati non rilevanti
            toExp_copy->mutex = NULL; //oscuro i dati non rilevanti
            //Mi servono nome, dimensione e contenuto
            //nome:
            toExp_copy->filename = malloc(strlen(toExp->filename)*sizeof(char)+1);
            if(toExp_copy->filename == NULL) return FATAL;
            toExp_copy->filename[strlen(toExp->filename)] = '\0';
            strcpy(toExp_copy->filename, toExp->filename);
            //dimensione:
            toExp_copy->size = toExp->size;
            //contenuto:
            toExp_copy->content = malloc(toExp_copy->size);
            if(toExp_copy->content == NULL) return FATAL;
            memcpy(toExp_copy->content, toExp->content, toExp_copy->size);

            //Aggiungo alla lista
            list_append(*list, toExp_copy);

            //Modifico lo storage per l'eliminazione
            storage->occupied_memory -= toExp->size;
            storage->files_number--;
            storage->times_replacement_algorithm++;
            //Lo elimino dallo storage
            if (icl_hash_delete(storage->files, toExp->filename, free, (void*)file_destroy) != 0) return FATAL;
            if(exp_file_name) free(exp_file_name);
            if(elem) free(elem);
        }while(true);

        //Eseguo l'append
        append:
        if ( (toAppend->content = realloc(toAppend->content, toAppend->size + size)) == NULL)
            return FATAL;
        memcpy(toAppend->content+toAppend->size, data, size);
        toAppend->size = toAppend->size+size;
        //modifico variabili dello storage
        storage->occupied_memory += size;
        if(storage->occupied_memory > storage->max_occupied_memory)
            storage->max_occupied_memory = storage->occupied_memory;

    }else{
        //Non si hanno i permessi per accedere al file
        if(rwlock_writerUnlock(toAppend->mutex) != 0) return FATAL;
        if(rwlock_writerUnlock(storage->mutex) != 0) return FATAL;
        errno = EPERM;
        return FAILURE;
    }
    if(rwlock_writerUnlock(toAppend->mutex) != 0) return FATAL;
    if(rwlock_writerUnlock(storage->mutex) != 0) return FATAL;
    return SUCCESS;
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
        errno = EACCES;
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

    errno = EPERM;
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
        errno = EACCES;
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
        errno = EPERM;
        return FAILURE;
    }

    //inutilizzato
    return FAILURE;
}

int storage_closeFile(storage_t* storage, char* filename, int client){

    if(!storage || !filename){
        errno = EINVAL;
        return FAILURE;
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

int storage_removeFile(storage_t* storage, char* filename, int client, unsigned int* deleted_bytes){

    if(!storage || !filename){
        errno = EINVAL;
        return FAILURE;
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
            *deleted_bytes = toRemove->size;
            //Se il file aveva effettivamente un contenuto allora modifico i numero di file e la memoria occupata
            storage->occupied_memory -= toRemove->size;
            storage->files_number--;
            elem_t* toRemove_elem = list_remove_elem(storage->filenames_queue, filename, (void*)strcmp);
            if(!toRemove_elem) return FATAL;
            free(toRemove_elem->content);
            free(toRemove_elem);
        }
        //Elimino dalla tabella dei file
        if (icl_hash_delete(storage->files, filename, free, (void*)file_destroy) != 0) return FATAL;
        //Rilascio la lock sullo storage solo alla fine
        if(rwlock_writerUnlock(storage->mutex) != 0) return FATAL;

        return SUCCESS;

    }else{//Il client non è il locker
        if(rwlock_writerUnlock(toRemove->mutex) != 0) return FATAL;
        if(rwlock_writerUnlock(storage->mutex) != 0) return FATAL;
        errno = EPERM;
        return FAILURE;
    }
}

// Funzioni di supporto //

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

    if(i == MAX_CONN){
        //significa che non ho trovato spazio per aggiungere il client
        errno = EUSERS;
        return FAILURE;
    }

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

    errno = ENODATA;
    return FAILURE;
}

int storage_printStats(storage_t* storage){

    if(!storage)
        return FAILURE;

    printf("\n=========== STATISTICHE FILE STORAGE ===============\n");
    printf("Numero massimo di file raggiunto: %d\n", storage->max_files_number);
    printf("Dimensione massima della memoria occupata %f MB\n", ((double)storage->max_occupied_memory)/1024.0/1024.0 );
    printf("Algoritmo di rimpiazzamento eseguito %d volte\n", storage->times_replacement_algorithm);
    printf("----- FILES (%d) ------\n", storage->files_number);
    list_printAsString(storage->filenames_queue);
    printf("----------------------\n");
    printf("\n====================================================\n");

    return SUCCESS;

}
