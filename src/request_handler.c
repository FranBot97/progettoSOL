//
// Created by linuxlite on 22/02/22.
//
#include <unistd.h>
#include <request_handler.h>
#include <stdlib.h>
#include <storage.h>
#include <util.h>
#include <string.h>
#include <errno.h>
#include <request.h>

void request_handler_function(request_t* request) {

    if (!request)
        return;

    //Se la richiesta è valida qui la gestisco
    printf("Richiesta %d da parte del client %ld \n", request->opcode, request->client_fd);
    fflush(stdout);

    int result = -1;
    /***** Identifico la richiesta e effettuo le operazioni corrispondenti ******/
    switch(request->opcode){
        case OPEN:
            result = server_openFile(request);
            break;
        case WRITE:
            result = server_writeFile(request);
            goto check_fatal;
            break;
        case APPEND:
            result = server_appendToFile(request);
            goto check_fatal;
            break;
        case READ:
            result = server_readFile(request);
            goto check_fatal;
            break;
        case READN:
            result = server_readNFiles(request);
            goto check_fatal;
            break;
        case LOCK:
            result = server_lockFile(request);
            break;
        case CLOSE:
            result = server_closeFile(request);
            break;
        case UNLOCK:
            result = server_unlockFile(request);
            break;
        case REMOVE:
            result = server_removeFile(request);
            break;
        default: //invalid request
            errno = EBADRQC;
            break;
    }

    //Invio codice del risultato e errno al client
    int errno_copy = errno;
    writen(request->client_fd, &result, sizeof(int));
    writen(request->client_fd, &errno_copy, sizeof(int));
    //Gli errori che lascerebbero lo storage inconsistente (ad es. fallimento di malloc, lock/unlock mutex)
    //vengono marcati come fatali e non permettono al server di continuare
    check_fatal:
    if(result == FATAL){
        exit(FATAL);
    }

    //Se l'operazione è una LOCK fallita NON rimetto il client tra i descrittori in attesa
    //ma lo aggiungo ad un array di client che attendono di fare una lock
    if(request->opcode == LOCK && result == FAILURE && errno_copy == EACCES){
        if (server_addClientWaiting(request) != 0) exit(FATAL);
        goto cleanup;
    }

    //Comunico al thread main il file descriptor del client da analizzare di nuovo
    //soltanto se l'operazione non è una LOCK fallita
    if (write(request->pipe_write, &request->client_fd, sizeof(int)) <= 0) {
        perror("Errore scrittura pipe\n");
        exit(FATAL);
    }

    if(request->opcode == UNLOCK && result == 0){
        //Se è una unlock andata a buon fine posso riprovare le lock in attesa
        int result_2;
        result_2 = server_removeClientWaiting(request);
        if(result_2 == FAILURE) goto cleanup;
        if (write(request->pipe_write, &result_2, sizeof(int)) <= 0) {
            perror("Errore scrittura pipe\n");
            exit(FATAL);
        }
    }


    cleanup:
    free(request);
    request = NULL;
}

int server_openFile(request_t* request) {
    char OP[MAX_STRLEN]; //Usato per la stampa nel file di log
    //Leggo dal client tutti i dati che mi servono, nell'ordine: flags di apertura, lunghezza nome del file, nome del file
    int client_fd = (int)request->client_fd;
    size_t filename_len = 0;
    char* filename = NULL;
    char filename_copy[MAX_FILENAME];
    int flags;
    int result = -1;

    if (readn(client_fd, &flags, sizeof(int)) <= 0) goto cleanup;
    if (readn(client_fd, &filename_len, sizeof(size_t)) <= 0) goto cleanup;
    if (filename_len > MAX_FILENAME){
        errno = ENAMETOOLONG;
        goto cleanup;
    }
    if(filename_len <= 0){
        errno = EINVAL;
        goto cleanup;
    }
    filename = (char*)malloc(filename_len*sizeof(char)+1);
    if(filename == NULL) goto cleanup;
    if (readn(client_fd, (void*)filename, filename_len) <= 0) goto cleanup;
    filename[filename_len] = '\0';
    strcpy(filename_copy, filename);
    //Chiamo la rispettiva funzione della classe "storage" che effettuerà l'operazione
    result = storage_openFile(request->storage, filename, flags, client_fd);

    cleanup:
    if(flags & O_LOCK) strcpy(OP, "OPENLOCK");
    else strcpy(OP, "OPEN");
    write_logfile(request, OP, client_fd, 0, 0, 0, filename_copy, (result == 0 ? "OK" : "ERR"));

    if(result == 0)
        return result;
    else{
        if(filename)
            free(filename);
        return result;
    }

}

int server_writeFile(request_t* request){

    int client_fd = (int)request->client_fd;
    int result = -1;
    unsigned long sent_bytes = 0; //file di log

    //Leggo dal client nell'ordine: lunghezza nome file, nome file, dimensione del file in bytes, contenuto
    int filename_len; char* filename = NULL; off_t file_size; void* file_data = NULL;
    if (readn(client_fd, &filename_len, sizeof(size_t)) <= 0) goto cleanup;
    filename = (char*)malloc(filename_len*sizeof(char)+1);
    if(!filename) return FATAL;
    filename[filename_len] = '\0';
    if (readn(client_fd, filename, filename_len) <= 0) goto cleanup;
    if (readn(client_fd, &file_size, sizeof(off_t)) <= 0) goto cleanup;
    file_data = malloc(file_size);
    if(!file_data) return FATAL;
    if (readn(client_fd, file_data, file_size) <= 0) goto cleanup;

    //Chiamo la rispettiva funzione della classe "storage" che effettuerà l'operazione
    list_t* expelled_files = NULL;
    result = storage_writeFile(request->storage, filename, file_size, file_data, client_fd, &expelled_files);

    //Comunico al client se la scrittura è andata a buon fine
    int errno_copy = errno;
    if( writen(request->client_fd, &result, sizeof(int))  <= 0) goto cleanup;
    if( writen(request->client_fd, &errno_copy, sizeof(int)) <= 0) goto cleanup;
    if(result == FATAL){
        exit(FATAL);
    }
    //Se non è andata a buon fine esco
    if(result != 0)
        goto  cleanup;


    //Controllo se ci sono file da inviare
    size_t exp_file_size;
    if(expelled_files != NULL){
        while(expelled_files->num_elem > 0){
            elem_t* elem = list_remove_head(expelled_files);
            file_t* exp_file = elem->content;
            exp_file_size = exp_file->size;
            size_t exp_file_name_len = strlen(exp_file->filename);
            //Invio la dimensione del file
            if( writen(request->client_fd, &exp_file_size, sizeof(size_t)) <= 0) goto cleanup;
            //Invio i restanti dati del file, nell'ordine: lunghezza nome file, nome file, contenuto
            if( writen(request->client_fd, &exp_file_name_len, sizeof(size_t)) <= 0) goto cleanup;
            if( writen(request->client_fd, exp_file->filename, exp_file_name_len) <= 0) goto cleanup;
            if( writen(request->client_fd, exp_file->content, exp_file_size) <= 0) goto cleanup;

            //Scrivo nel file di log
            write_logfile(request, "VICTIM", client_fd, 0, 0, exp_file_size,
                          filename,"OK");

            sent_bytes+=exp_file_size;
            file_destroy(exp_file);
            //if(elem->content) free(elem->content);
            if(elem) free(elem);
        }
        list_destroy(expelled_files, NULL);
    }
    //Invio 0 per dire al client che non ci sono più file
    exp_file_size = 0;
    if( writen(request->client_fd, &exp_file_size, sizeof(size_t)) <= 0) goto cleanup;


    cleanup:
    //Scrivo nel file di log
    write_logfile(request, "WRITE", client_fd, 0, 0, sent_bytes,
                  filename,(result == 0 ? "OK" : "ERR"));
    if(filename)
        free(filename);
    if(file_data && result == -1)
        free(file_data);
    return result;

}

int server_appendToFile(request_t* request){
    int client_fd = (int)request->client_fd;
    int result = -1;
    unsigned long sent_bytes = 0;

    //Leggo dal client nell'ordine: lunghezza nome file, nome file, dimensione del contenuto in bytes, contenuto
    int filename_len; char* filename = NULL; size_t size; void* data = NULL;
    if (readn(client_fd, &filename_len, sizeof(size_t)) <= 0) goto cleanup;
    filename = (char*)malloc(filename_len*sizeof(char)+1);
    if(!filename) return FATAL;
    filename[filename_len] = '\0';
    if (readn(client_fd, filename, filename_len) <= 0) goto cleanup;
    if (readn(client_fd, &size, sizeof(size_t)) <= 0) goto cleanup;
    data = malloc(size);
    if(!data) return FATAL;
    if (readn(client_fd, data, size) <= 0) goto cleanup;

    //Chiamo la rispettiva funzione della classe "storage" che effettuerà l'operazione
    list_t* expelled_files = NULL;
    result = storage_appendToFile(request->storage, filename, size, data, client_fd, &expelled_files);

    //Comunico al client se la scrittura è andata a buon fine
    int errno_copy = errno;
    if( writen(request->client_fd, &result, sizeof(int))  <= 0) goto cleanup;
    if( writen(request->client_fd, &errno_copy, sizeof(int)) <= 0) goto cleanup;
    if(result == FATAL){
        exit(FATAL);
    }
    //Se non è andata a buon fine esco
    if(result != 0) goto  cleanup;

    //Controllo se ci sono file da inviare
    size_t exp_file_size;
    if(expelled_files != NULL){
        while(expelled_files->num_elem > 0){
            elem_t* elem = list_remove_head(expelled_files);
            file_t* exp_file = elem->content;
            exp_file_size = exp_file->size;
            size_t exp_file_name_len = strlen(exp_file->filename);
            //Invio la dimensione del file
            if( writen(request->client_fd, &exp_file_size, sizeof(size_t)) <= 0) goto cleanup;
            //Invio i restanti dati del file, nell'ordine: lunghezza nome file, nome file, contenuto
            if( writen(request->client_fd, &exp_file_name_len, sizeof(size_t)) <= 0) goto cleanup;
            if( writen(request->client_fd, exp_file->filename, exp_file_name_len) <= 0) goto cleanup;
            if( writen(request->client_fd, exp_file->content, exp_file_size) <= 0) goto cleanup;
            //Scrivo nel file di log
            write_logfile(request, "VICTIM", client_fd, 0, 0, exp_file_size,
                          filename,"OK");
            sent_bytes+=exp_file_size;
            file_destroy(exp_file);
            //if(elem->content) free(elem->content);
            if(elem) free(elem);
        }
        list_destroy(expelled_files, NULL);
    }
    //Invio 0 per dire al client che non ci sono più file
    exp_file_size = 0;
    if( writen(request->client_fd, &exp_file_size, sizeof(size_t)) <= 0) goto cleanup;


    cleanup:
    //Scrivo nel file di log
    write_logfile(request, "WRITE", client_fd, 0, 0, sent_bytes,
                  filename,(result == 0 ? "OK" : "ERR"));
    if(filename)
        free(filename);
    if(data)
        free(data);
    return result;

}

int server_readFile(request_t* request){

    int client_fd = (int)request->client_fd;
    long long result = -1;
    void* file_content = NULL;
    size_t file_size = 0;

    //Leggo dal client nell'ordine: lunghezza nome file, nome file
    size_t filename_len;
    char* filename = NULL;
    if (readn(client_fd, &filename_len, sizeof(size_t)) <= 0) goto cleanup;
    filename = (char*)malloc(filename_len*sizeof(char)+1);
    if(!filename) return FATAL;
    filename[filename_len] = '\0';
    if (readn(client_fd, filename, filename_len) <= 0) goto cleanup;

    //Chiamo la rispettiva funzione della classe "storage" che effettuerà l'operazione
    result = storage_readFile(request->storage, filename, client_fd, &file_content);

    //Se result <= 0 invio l'errore al client e esco
    if(result <= 0){
        if (writen(client_fd, &result, sizeof(long long)) <= 0) goto cleanup;
        goto cleanup;
    }

    //Altrimenti result rappresenta il numero di bytes letti
    file_size = result;
    // Invio il contenuto del file al client, nell'ordine: dimensione e contenuto
    if (writen(client_fd, &file_size, sizeof(size_t)) <= 0) goto cleanup;
    if (writen(client_fd, file_content, file_size) <= 0) goto cleanup;

    cleanup:
    //Scrivo nel file di log
    write_logfile(request, "READ", client_fd, 0, 0, result,
                  filename,(result > 0 ? "OK" : "ERR"));
    if(filename) free(filename);
    if(file_content) free(file_content);
    return (int)result;
}

int server_readNFiles(request_t* request){

    int client = (int)request->client_fd;
    int N = 0;
    int result = -1;
    list_t* files_to_send = NULL;
    //Leggo dal client il numero di file che vuole leggere
    if(readn(client, &N, sizeof(int)) <= 0) goto cleanup;
    //Chiamo la rispettiva funzione dello storage che inserirà nella lista i file
    result = storage_readNFiles(request->storage, client, N, &files_to_send);
    int errno_copy = errno;
    //Se è andata a buon fine result contiene il numero di file letti
    //e la lista files_to_send contiene i file
    if(result < 0) goto cleanup;
    elem_t* elem = list_remove_head(files_to_send);
    while(elem != NULL){
        file_t* file = elem->content;
        //Invio nell'ordine al client: dimensione file, contenuto, lunghezza nome file e nome file
        if( writen(request->client_fd, &file->size, sizeof(size_t))  <= 0) goto cleanup;
        if( writen(request->client_fd, file->content, file->size)  <= 0) goto cleanup;
        size_t file_name_len = strlen(file->filename);
        if( writen(request->client_fd, &file_name_len, sizeof(size_t))  <= 0) goto cleanup;
        if( writen(request->client_fd, file->filename, file_name_len)  <= 0) goto cleanup;
        //Per ogni lettura scrivo nel file di log
        write_logfile(request, "READ_N", client, 0, 0, file->size,
                      file->filename,"OK");
        file_destroy(file);
        free(elem);
        elem = list_remove_head(files_to_send);
    }
    //Invio dimensione non valida (= 0) per dire al client che non ci sono altri file
    size_t end = 0;
    if (writen(request->client_fd, &end, sizeof(size_t)) <= 0) return FAILURE;
    if(files_to_send) list_destroy(files_to_send, (void (*)(void *)) file_destroy);
    cleanup:
    //Scrivo nel file di log se c'è stato un errore
    if(result < 0) write_logfile(request, "READ_N", client, 0, 0, 0,
                  0,"ERR");
    //Invio result e poi errno
    if (writen(request->client_fd, &result, sizeof(int)) <= 0) return FAILURE;
    if (writen(request->client_fd, &errno_copy, sizeof(int)) <= 0) return FAILURE;

    return SUCCESS;
}

int server_closeFile(request_t* request){

    //Leggo dal client tutti i dati che mi servono, nell'ordine: lunghezza nome del file, nome del file
    int client_fd = (int)request->client_fd;
    size_t filename_len = 0;
    char* filename = NULL;
    int result = -1;

    if (readn(client_fd, &filename_len, sizeof(size_t)) <= 0) goto cleanup;
    if (filename_len > MAX_FILENAME){
        errno = ENAMETOOLONG;
        goto cleanup;
    }
    if(filename_len <= 0){
        errno = EINVAL;
        goto cleanup;
    }
    filename = (char*)malloc(filename_len*sizeof(char)+1);
    if(filename == NULL) return FATAL;
    if (readn(client_fd, (void*)filename, filename_len) <= 0) goto cleanup;
    filename[filename_len] = '\0';
    //Chiamo la rispettiva funzione della classe "storage" che effettuerà l'operazione
    result = storage_closeFile(request->storage, filename, client_fd);

    cleanup:
    //Scrivo nel file di log
    write_logfile(request, "CLOSE", client_fd, 0, 0, 0,
                  filename,(result == 0 ? "OK" : "ERR"));
    if(filename)
        free(filename);
    return result;

}

int server_lockFile(request_t* request){

    //Leggo dal client tutti i dati che mi servono, nell'ordine: lunghezza nome del file, nome del file
    int client_fd = (int)request->client_fd;
    size_t filename_len = 0;
    char* filename = NULL;
    int result = -1;

    if (readn(client_fd, &filename_len, sizeof(size_t)) <= 0) goto cleanup;
    if (filename_len > MAX_FILENAME){
        errno = ENAMETOOLONG;
        goto cleanup;
    }
    if(filename_len <= 0){
        errno = EINVAL;
        goto cleanup;
    }
    filename = (char*)malloc(filename_len*sizeof(char)+1);
    if(filename == NULL) return FATAL;
    if (readn(client_fd, (void*)filename, filename_len) <= 0) goto cleanup;
    filename[filename_len] = '\0';
    //Chiamo la rispettiva funzione della classe "storage" che effettuerà l'operazione
    result = storage_lockFile(request->storage, filename, client_fd);

    //Scrivo nel file di log
    write_logfile(request, "LOCK", client_fd, 0, 0, 0,
                  filename,(result == 0 ? "OK" : "ERR"));

    if(result == 0) {
        free(filename);
        return result;
    }

    cleanup:
    if(filename)
        free(filename);
    return result;
}

int server_unlockFile(request_t* request){

    //Leggo dal client tutti i dati che mi servono, nell'ordine: lunghezza nome del file, nome del file
    int client_fd = (int)request->client_fd;
    size_t filename_len = 0;
    char* filename = NULL;
    int result = -1;

    if (readn(client_fd, &filename_len, sizeof(size_t)) <= 0) goto cleanup;
    if (filename_len > MAX_FILENAME){
        errno = ENAMETOOLONG;
        goto cleanup;
    }
    if(filename_len <= 0){
        errno = EINVAL;
        goto cleanup;
    }
    filename = (char*)malloc(filename_len*sizeof(char)+1);
    if(filename == NULL) return FATAL;
    if (readn(client_fd, (void*)filename, filename_len) <= 0) goto cleanup;
    filename[filename_len] = '\0';
    //Chiamo la rispettiva funzione della classe "storage" che effettuerà l'operazione
    result = storage_unlockFile(request->storage, filename, client_fd);

    //Scrivo nel file di log
    write_logfile(request, "UNLOCK", client_fd, 0, 0, 0,
                  filename,(result == 0 ? "OK" : "ERR"));

    if(result == 0) {
        free(filename);
        return result;
    }

    cleanup:
    if(filename)
        free(filename);
    return result;
}

int server_removeFile(request_t* request){

    //Leggo dal client tutti i dati che mi servono, nell'ordine: lunghezza nome del file, nome del file
    int client_fd = (int)request->client_fd;
    size_t filename_len = 0;
    char* filename = NULL;
    int result = -1;
    unsigned int deleted_bytes = 0; //log file

    if (readn(client_fd, &filename_len, sizeof(size_t)) <= 0) goto cleanup;
    if (filename_len > MAX_FILENAME){
        errno = ENAMETOOLONG;
        goto cleanup;
    }
    if(filename_len <= 0){
        errno = EINVAL;
      goto cleanup;
    }
    filename = (char*)malloc(filename_len*sizeof(char)+1);
    if(filename == NULL) return FATAL;
    if (readn(client_fd, (void*)filename, filename_len) <= 0) goto cleanup;
    filename[filename_len] = '\0';
    //Chiamo la rispettiva funzione della classe "storage" che effettuerà l'operazione
    result = storage_removeFile(request->storage, filename, client_fd, &deleted_bytes);

    //Scrivo nel file di log
    write_logfile(request, "REMOVE", client_fd, deleted_bytes, 0, 0,
                  filename,(result == 0 ? "OK" : "ERR"));

    if(result == 0) {
        free(filename);
        return result;
    }

    cleanup:
    if(filename)
        free(filename);
    return result;

}

int server_addClientWaiting(request_t* request){
    return storage_addClientWaiting(request->storage, (int)request->client_fd);
}

int server_removeClientWaiting(request_t* request){
    return storage_removeClientWaiting(request->storage);
}

int write_logfile(request_t* request, const char* OP, int IDCLIENT, unsigned int DELETED_BYTES, unsigned int ADDED_BYTES,
                   unsigned int SENT_BYTES, const char* OBJECT_FILE, const char* OUTCOME){

    FILE* logfile = request->logfile;
    pthread_mutex_t* mutex = request->logfile_mutex;

    if(pthread_mutex_lock(mutex) != 0) return FATAL;
    if (fprintf(logfile, "THREAD=%lu,OP=%s,IDCLIENT=%d,DELETED_BYTES=%u,ADDED_BYTES=%u,SENT_BYTES=%u,OBJECT_FILE=%s,OUTCOME=%s\n",
            pthread_self(),OP, IDCLIENT, DELETED_BYTES, ADDED_BYTES, SENT_BYTES, OBJECT_FILE, OUTCOME) < 0) return FATAL;
    if(pthread_mutex_unlock(mutex) != 0) return FATAL;

    return SUCCESS;

}