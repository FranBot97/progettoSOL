//
// Created by linuxlite on 22/02/22.
//
#include <unistd.h>
#include <request_handler.h>
#include <stdlib.h>
#include <storage.h>
#include <pthread.h>
#include <util.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <request.h>
#include <stdbool.h>

//TODO sistemare con una unica write OK write ERROR
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
        case READ:
            break;
        case READN:
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
        };
    }

    cleanup:
    free(request);
    request = NULL;
}

int server_openFile(request_t* request) {
    //Leggo dal client tutti i dati che mi servono, nell'ordine: flags di apertura, lunghezza nome del file, nome del file
    int client_fd = (int)request->client_fd;
    size_t filename_len = 0;
    char* filename = NULL;
    int flags;
    int result = -1;

    if (readn(client_fd, &flags, sizeof(int)) <= 0) return -1;
    if (readn(client_fd, &filename_len, sizeof(size_t)) <= 0) return -1;
    if (filename_len > MAX_FILENAME){
        errno = ENAMETOOLONG;
        return -1;
    }
    if(filename_len <= 0){
        errno = EINVAL;
        return -1;
    }
    filename = (char*)malloc(filename_len*sizeof(char)+1);
    if(filename == NULL) return -1;
    if (readn(client_fd, (void*)filename, filename_len) <= 0) goto cleanup;
    filename[filename_len] = '\0';
    //Chiamo la rispettiva funzione della classe "storage" che effettuerà l'operazione
    result = storage_openFile(request->storage, filename, flags, client_fd);

    if(result == 0)
        return result;

    cleanup:
        if(filename)
            free(filename);
        return result;
}

int server_closeFile(request_t* request){

    //Leggo dal client tutti i dati che mi servono, nell'ordine: lunghezza nome del file, nome del file
    int client_fd = (int)request->client_fd;
    size_t filename_len = 0;
    char* filename = NULL;
    int result = -1;

    if (readn(client_fd, &filename_len, sizeof(size_t)) <= 0) return -1;
    if (filename_len > MAX_FILENAME){
        errno = ENAMETOOLONG;
        return -1;
    }
    if(filename_len <= 0){
        errno = EINVAL;
        return -1;
    }
    filename = (char*)malloc(filename_len*sizeof(char)+1);
    if(filename == NULL) return -1;
    if (readn(client_fd, (void*)filename, filename_len) <= 0) goto cleanup;
    filename[filename_len] = '\0';
    //Chiamo la rispettiva funzione della classe "storage" che effettuerà l'operazione
    result = storage_closeFile(request->storage, filename, client_fd);

    cleanup:
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

    if (readn(client_fd, &filename_len, sizeof(size_t)) <= 0) return -1;
    if (filename_len > MAX_FILENAME){
        errno = ENAMETOOLONG;
        return -1;
    }
    if(filename_len <= 0){
        errno = EINVAL;
        return -1;
    }
    filename = (char*)malloc(filename_len*sizeof(char)+1);
    if(filename == NULL) return -1;
    if (readn(client_fd, (void*)filename, filename_len) <= 0) goto cleanup;
    filename[filename_len] = '\0';
    //Chiamo la rispettiva funzione della classe "storage" che effettuerà l'operazione
    result = storage_lockFile(request->storage, filename, client_fd);

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

    if (readn(client_fd, &filename_len, sizeof(size_t)) <= 0) return -1;
    if (filename_len > MAX_FILENAME){
        errno = ENAMETOOLONG;
        return -1;
    }
    if(filename_len <= 0){
        errno = EINVAL;
        return -1;
    }
    filename = (char*)malloc(filename_len*sizeof(char)+1);
    if(filename == NULL) return -1;
    if (readn(client_fd, (void*)filename, filename_len) <= 0) goto cleanup;
    filename[filename_len] = '\0';
    //Chiamo la rispettiva funzione della classe "storage" che effettuerà l'operazione
    result = storage_unlockFile(request->storage, filename, client_fd);

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

    if (readn(client_fd, &filename_len, sizeof(size_t)) <= 0) return -1;
    if (filename_len > MAX_FILENAME){
        errno = ENAMETOOLONG;
        return -1;
    }
    if(filename_len <= 0){
        errno = EINVAL;
        return -1;
    }
    filename = (char*)malloc(filename_len*sizeof(char)+1);
    if(filename == NULL) return -1;
    if (readn(client_fd, (void*)filename, filename_len) <= 0) goto cleanup;
    filename[filename_len] = '\0';
    //Chiamo la rispettiva funzione della classe "storage" che effettuerà l'operazione
    result = storage_removeFile(request->storage, filename, client_fd);

    if(result == 0) {
        free(filename);
        return result;
    }

    cleanup:
    if(filename)
        free(filename);
    return result;

}

int server_writeFile(request_t* request){

    int client_fd = (int)request->client_fd;

    //Leggo dal client nell'ordine: lunghezza nome file, nome file, dimensione del file in bytes, contenuto
    int filename_len; char* filename; size_t file_size; void* file_data;
    if (readn(client_fd, &filename_len, sizeof(int)) <= 0) return -1;
    filename = (char*)malloc(filename_len*sizeof(char)+1);
    filename[filename_len] = '\0';
    if (readn(client_fd, &filename, filename_len) <= 0) return -1;
    if (readn(client_fd, &file_size, sizeof(size_t)) <= 0) return -1;
    file_data = malloc(file_size);
    if (readn(client_fd, file_data, file_size) <= 0) return -1;

    //Chiamo la rispettiva funzione della classe "storage" che effettuerà l'operazione
    int result = -1;
    list_t* expelled_files = NULL;
    result = storage_writeFile(request->storage, filename, file_size, file_data, client_fd, expelled_files);

    //Comunico al client se la scrittura è andata a buon fine
    int errno_copy = errno;
    if( writen(request->client_fd, &result, sizeof(int))  <= 0) return FATAL;
    if( writen(request->client_fd, &errno_copy, sizeof(int)) <= 0) return FATAL;
    if(result == FATAL){
        exit(FATAL);
    }
    //Se non è andata a buon fine esco
    if(result != 0) goto  cleanup;

    //Controllo se ci sono file da inviare
    size_t exp_file_size;
    if(expelled_files == NULL){
        while(expelled_files->num_elem > 0){
            elem_t* elem = list_remove_head(expelled_files);
            file_t* exp_file = elem->content;
            exp_file_size = exp_file->size;
            size_t exp_file_name_len = strlen(exp_file->filename);
            //Invio la dimensione del file
            if( writen(request->client_fd, &exp_file_size, sizeof(size_t)) <= 0) return FATAL;
            //Invio i restanti dati del file, nell'ordine: lunghezza nome file, nome file, contenuto
            if( writen(request->client_fd, &exp_file_name_len, sizeof(size_t)) <= 0) return FATAL;
            if( writen(request->client_fd, exp_file->filename, exp_file_name_len) <= 0) return FATAL;
            if( writen(request->client_fd, exp_file->content, exp_file_size) <= 0) return FATAL;

            file_destroy(exp_file);
            if(elem->content) free(elem->content);
            if(elem) free(elem);
        }
    }
    //Invio 0 per dire al client che non ci sono più file
    exp_file_size = 0;
    if( writen(request->client_fd, &exp_file_size, sizeof(size_t)) <= 0) return FATAL;

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