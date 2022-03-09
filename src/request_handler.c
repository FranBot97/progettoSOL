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

//TODO ero rimasto a fare i flag per Open File O_LOCK O_CREATE quando usarli ecc..

void request_handler_function(request_t* request) {

    if (!request)
        return;

    //Se la richiesta è valida qui la gestisco
    else {
     //   storage_t* myStorage = request->myStorage;
        printf("Il messaggio ricevuto è %s\n", request->command);
        fflush(stdout);

        /*******OPERAZIONI STORAGE, QUI CI VANNO LE FUNZIONI IN BASE ALLA RICHIESTA*****/

      if(strncmp(request->command, "OPEN_FILE",strlen("OPEN_FILE") ) == 0){
          char* filename = NULL;
          if(readClient((void*)&filename, request->client_fd, "string") == 0){
              int err;
              if ( (err = openFile(request, filename) ) == 0) {
                  writeClient("OK - File aperto correttamente", request->client_fd);
              }
              else if(err == 1)
                  writeClient("ERROR - Un altro utente ha già fatto la lock su questo file", request->client_fd);
              else {
                  writeClient("ERROR - Impossibile aprire il file", request->client_fd);
              }
          }

      }else if(strcmp(request->command, "UNLOCK_FILE") == 0){
          char* filename = NULL;
          if(readClient((void*)&filename, request->client_fd, "string") == 0){
              int err;
              if ( (err = unlockFile(request, filename) ) == 0) {
                  writeClient("OK - File sbloccato correttamente", request->client_fd);
              }
              else if(err == 1)
                  writeClient("ERROR - Un altro utente ha fatto la lock su questo file", request->client_fd);
              else if(err == 2)
                  writeClient("ALREADY UNLOCKED - Il file è già stato liberato", request->client_fd);
              else {
                  writeClient("ERROR - Impossibile trovare il file", request->client_fd);
              }
          }
      }

        //Comunico al thread main il file descriptor del client da analizzare di nuovo
        if ( write(request->pipe_write, &request->client_fd, sizeof(int)) <= 0){
            printf("Errore scrittura pipe\n");
        };

        clean:
        free(request->command);
        free(request);

    }
}

int unlockFile(request_t* request, char* filename){

    if(!request || !filename)
        return -1;

    //TODO check lock errors
    manage_storage(&(request->myStorage->storage_mutex), "lock");
    if (storage_findFile(request->myStorage, filename) == NULL) {
        manage_storage(&(request->myStorage->storage_mutex), "unlock");
        return -1;
    }
    int return_value = (storage_unlockFile(request->myStorage, filename, request->client_fd));
    manage_storage(&(request->myStorage->storage_mutex), "unlock");
    return return_value;
}

int manage_storage(pthread_mutex_t* mutex, char* action){

    if (strcmp(action, "lock") == 0){
        if (pthread_mutex_lock(mutex) != 0) {
            //printf("ERRORE FATALE lock\n");
            return -1;
        }
        return 0;
    }

    if(strcmp(action, "unlock") == 0){
        if (pthread_mutex_unlock(mutex) != 0) {
            //printf("ERRORE FATALE unlock\n");
            return -1;
        }
        return 0;
    }

    return 1;
}

int openFile(request_t* request, char* pathname) {

    icl_hash_t *files = request->myStorage->files;
    pthread_mutex_t *storage_mutex = &(request->myStorage->storage_mutex);
    //pthread_mutex_t *files_mutex = request->files_mutex;
    char *open_file_option = request->command;

    //Controllo se è una richiesta con creazione
    if (strncmp(open_file_option, "OPEN_FILE_CREATE", strlen("OPEN_FILE_CREATE")) == 0) {
        //Prendo la lock sullo storage
        if (manage_storage(storage_mutex, "lock") == 0) {

            //Controllo se il file è già presente
            if (storage_findFile(request->myStorage, pathname) == NULL) {
                //faccio la unlock
                manage_storage(storage_mutex, "unlock");
                //Creo il file
                file_t *newFile;
                //controllo se c'è anche la lock
                if (strcmp(open_file_option, "OPEN_FILE_CREATE_LOCK") == 0)
                    newFile = file_create(pathname, 0, NULL, request->client_fd);
                else
                    newFile = file_create(pathname, 0, NULL, -1);

                if (!newFile) {
                    if(pathname) {
                        free(pathname);
                        pathname = NULL;
                    }
                    return -1;
                }
                //Ora lo aggiungo allo storage
                if (manage_storage(storage_mutex, "lock") == 0) {
                    //Ricontrollo la presenza del file nel caso in cui nel frattempo
                    // un altro utente fosse subentrato
                    //e avesse aggiunto un file con lo stesso nome
                    if(storage_findFile(request->myStorage, pathname) != NULL){
                        manage_storage(storage_mutex, "unlock");
                        if(pathname) {
                            free(pathname);
                            pathname = NULL;
                        }
                        errno = EXIT_FAILURE;
                        return -1;
                    }
                    list_t* expelledFiles = NULL;
                        if (storage_addfile(request->myStorage, newFile, pathname, &expelledFiles) == -1) {
                            manage_storage(storage_mutex, "unlock");
                            if(pathname) {
                                free(pathname);
                                pathname = NULL;
                            }
                            if(expelledFiles){
                                free(expelledFiles);
                                expelledFiles = NULL;
                            }
                            errno = EXIT_FAILURE;
                            return -1;
                        }
                        if(expelledFiles != NULL && expelledFiles->num_elem != 0){
                            //TODO INVIO I FILE ESPULSI AL CLIENT
                            int i = 0;
                            int max = expelledFiles->num_elem;
                            while(i < max) {
                                writeClient("EXPELLED", request->client_fd);
                                elem_t* toDestroy = get_tail(expelledFiles);
                                file_t* toSend = toDestroy->content;
                                writeClient(toSend->nome_file, request->client_fd);
                                writen(request->client_fd, &(toSend->dimensione_file), sizeof(int));
                                writen(request->client_fd, toSend->contenuto_file, toSend->dimensione_file);
                                i++;
                                file_clean(toSend);
                                free(toDestroy);
                            }
                            list_destroy(expelledFiles, (void*)file_clean);
                            writeClient("STOP", request->client_fd);
                        }
                    manage_storage(storage_mutex, "unlock");
                }
                return 0;

            } else { //Se il file è già presente ma è una richiesta di creazione allora esco con errore
                manage_storage(storage_mutex, "unlock");
                if(pathname) {
                    free(pathname);
                    pathname = NULL;
                }
                errno = EXIT_FAILURE;
                return -1;
            }

        }
        if(pathname) {
            free(pathname);
            pathname = NULL;
        }
        return -1;

    }

    //Richiesta di apertura file senza creazione
    else{

        if (manage_storage(storage_mutex, "lock") == 0) {

            //Controllo se il file è presente
            if (storage_findFile(request->myStorage, pathname) != NULL) {
                //controllo se c'è la lock
                if (strcmp(open_file_option, "OPEN_FILE_LOCK") == 0) {
                    int err;
                    //Faccio la lock del file
                    if ( (err = storage_lockFile(request->myStorage, pathname, request->client_fd)) != 0){
                        manage_storage(storage_mutex, "unlock");
                        if(pathname) {
                            free(pathname);
                            pathname = NULL;
                        }
                        if(err == -1)
                            return -1;
                        if(err == 1)
                            return 1;
                    }
                    manage_storage(storage_mutex, "unlock");
                    if(pathname) {
                        free(pathname);
                        pathname = NULL;
                    }
                    return 0;
                }
                //faccio la unlock
                manage_storage(storage_mutex, "unlock");
                if(pathname) {
                    free(pathname);
                    pathname = NULL;
                }
                return 0;

            }else{ //Se invece il file non è presente esco con errore
                manage_storage(storage_mutex, "unlock");
                if(pathname) {
                    free(pathname);
                    pathname = NULL;
                }
                errno = ENOENT;
                return -1;
            }
        }
    }
    if(pathname) {
        free(pathname);
        pathname = NULL;
    }
    return -1;
}

int readClient(void** content, int client_fd, const char* contentType){
    unsigned long len = 0;

    if (readn(client_fd, &len, sizeof(int) ) == -1){
        printf("Errore nel thread durante lettura da client %d\n", client_fd);
        fflush(stdout);
        return -1;
    }

    if(strcmp(contentType, "string") == 0)
        *content = (char*)calloc(len+1, sizeof(char));
    if(strcmp(contentType, "file") == 0)
        *content = calloc(len, 1);
    if(!*content)
        return -1;

    if (readn(client_fd, *content, len ) == -1){
        printf("Errore nel thread durante lettura da client %d\n", client_fd);
        fflush(stdout);
        free(*content);
        return -1;
    }


    return 0;


}


int readClientSingle(void* content, int client_fd, size_t size){
    fflush(stdout);
    if (readn(client_fd, content, size ) == -1){
        printf("Errore nel thread durante lettura da client %d\n", client_fd);
        return -1;
    }

    return 0;
}

int writeClient(char* msg, int client_fd){
    fflush(stdout);
    size_t len = strlen(msg);
    if (writen(client_fd, &len, sizeof(int) ) != 1){
        printf("Errore nel thread durante scrittura verso client %d\n", client_fd);
        return -1;
    }
    if (writen(client_fd, msg, len+1 ) != 1){
        printf("Errore nel thread durante scrittura verso client %d\n", client_fd);
        return -1;
    }

    return 0;
}
