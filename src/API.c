//
// Created by linuxlite on 01/03/22.
//
//TODO controlla strncpy strlen ecc.. e gli invii delle stringhe
#include <time.h>
#include <stdbool.h>
#include <list.h>
#include <unistd.h>
#include <sys/un.h>
#include <API.h>

static bool connected = false;
static int socket_fd = -1;
char mySockname[MAX_SOCKNAME] = "";
char last_file_opened[MAX_FILENAME] = "";
list_t* locked_files = NULL;

enum  operations_ {
    NULL_op = -1,
    openConnection_op = 0,
    openFile_simple_op = 1,
    openFile_with_CREATE_OR_LOCK_op = 2,
    closeConnection_op = 3,
    lockFile_op = 4,
    unlockFile_op = 5
};
enum operations_ last_op = NULL_op;

enum flags_   {
    O_CREATE = 1 << 0,
    O_LOCK  = 1 << 1
};

int openConnection(const char* sockname, int msec, const struct timespec abstime){

    if(connected) {
        errno = EISCONN;
        return -1;
    }

    if((sockname == NULL) || (msec < 0) ){
        errno = EINVAL;
        return -1;
    }

    if(strlen(sockname) > MAX_SOCKNAME){
        errno = ENAMETOOLONG;
        return -1;
    }

    socket_fd = socket(AF_UNIX,SOCK_STREAM,0); //sets errno
    if(socket_fd == -1) {
        return -1;
    }

    //Inizializzazione socket
    struct sockaddr_un serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, sockname, MAX_SOCKNAME-1);

    //prendo il tempo attuale
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);

    //Riprovo a connettermi finché non ho raggiunto "abstime"
    printf("Connessione al server in corso..\n");
    int isConnected = -1;
    while( (isConnected = connect(socket_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr) )) != 0
            && (now.tv_sec < abstime.tv_sec) ) {
        sleep(msec/1000); //Aspetto
        clock_gettime(CLOCK_REALTIME, &now); //Ricalcolo il tempo attuale
        printf("Tentativo di riconnessione..\n");
    }
    if(isConnected != 0)
      return -1;

    //Se va a buon fine inizializzo i parametri
    connected = true;
    printf("Connesso con successo\n");
    locked_files = list_create();
    strncpy(last_file_opened, "", MAX_FILENAME-1);
    strncpy(mySockname, sockname, MAX_SOCKNAME-1);
    last_op = openConnection_op;
    return 0;
}


int closeConnection(const char* sockname){

    if(connected == false){
        errno = ENOTCONN;
        return -1;
    }

    if(strlen(sockname) > MAX_SOCKNAME){
        errno = ENAMETOOLONG;
        return -1;
    }

    if(sockname == NULL || strncmp(sockname, mySockname, MAX_FILENAME) != 0){
        errno = EINVAL;
        return -1;
    }

    //libero tutti i file lockati dal client
    char filename[MAX_FILENAME];
    if(locked_files != NULL){
        while (locked_files->num_elem > 0) {
            elem_t* toDestroy = get_head(locked_files);
            strncpy(filename, toDestroy->content, MAX_FILENAME-1);
            //printf("%s\n", filename);
            unlockFile(filename);
            free(toDestroy);

            //aspetto responso
           // pointer = pointer->next;
        }
    }

    if (close(socket_fd) != 0 )
        return -1;

    reset();
    return 0;
}

int openFile(const char* pathname, int flags){

    if(connected == false) {
        errno = ENOTCONN;
        return -1;
    }
    if(!pathname){
        errno = EINVAL;
        return -1;
    }
    if(strlen(pathname) > MAX_FILENAME){
        errno = ENAMETOOLONG;
        return -1;
    }

    char* request;
    enum operations_ op = openFile_with_CREATE_OR_LOCK_op;
    //Controllo se ho settato sia O_CREATE che O_LOCK
    if( (flags & O_CREATE) && (flags & O_LOCK) ){
        request = "OPEN_FILE_CREATE_LOCK";
    } //Altrimenti guardo se c'è solo O_CREATE
    else if(flags & O_CREATE){
        request = "OPEN_FILE_CREATE";
        }
    //Altrimenti guardo se c'è solo O_LOCK
    else if(flags & O_LOCK){
        request = "OPEN_FILE_LOCK";
        }
    //Nessun flag settato
    else{
        request = "OPEN_FILE";
        op = openFile_simple_op;
    }

    //Invio la richiesta al server
    if (sendToServer(request, strlen(request),"request") != 0) {
        return -1;
    }

    //Invio anche il nome del file
    unsigned long len = strlen(pathname);
    if (sendToServer((void*)pathname, len, "string") != 0){
        return -1;
    }

    //Aspetto risposta
    char* response = recieveFromServer("string"); /*BLOCCATO QUI DOPO CHE SI FA UNLOCK*/
    if(response == NULL)
        return -1;

   if(strncmp(response, "EXPELLED", 8) == 0){
        char* filename = recieveFromServer("string");
        void* file_content = recieveFromServer("file");
        printf("File espluso %s\n", filename);
        //TODO salvare il file in locale
        response = recieveFromServer("string");
        while(strcmp(response, "STOP") != 0){ //TODO file che si chiama OK? no forse non può succedere perché leggo la risp e poi i file
            filename = recieveFromServer("string");
            file_content = recieveFromServer("file");
            printf("File espluso %s\n", filename);
            //TODO salvare il file in locale
            response = recieveFromServer("string");
        }
    }

    if(strncmp(response, "OK", 2) == 0) {
        strcpy(last_file_opened, pathname);
        if(flags & O_LOCK)
            add_head_element(locked_files, (void*)pathname);
        last_op = op;
        free(response);
        return 0;
    }//Se ho ricevuto dei file espulsi
    else {
        free(response); //TODO errno
        return -1;
    }

    return 0;
}

int readFile(const char* pathname, void** buf, size_t* size){

    return 0;
}

int readNFiles(int N, const char* dirname){
    return 0;
}

int writeFile(const char* pathname, const char* dirname){
    return 0;
}

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname){
    return 0;
}

int lockFile(const char* pathname){
    //TODO send request to server and if ok modify API list
    add_head_element(locked_files, (char*)pathname);
    return 0;
}

int unlockFile(const char* pathname){
    printf("unlock file start\n");
    sendToServer("UNLOCK_FILE", strlen("UNLOCK_FILE")+1,"string");
    sendToServer((void*)pathname, strlen(pathname)+1, "string");

    char* response = recieveFromServer("string");
    if(response == NULL)
        return -1;

    if(strncmp(response, "OK", 2) == 0) {
        last_op = unlockFile_op;
        free(response);
        elem_t * toDestroy = get_elem(locked_files, (char*)pathname, (int (*)(void *, void *)) strcmp);
        free(toDestroy);
        return 0;
    }else if(strncmp(response, "ALREADY UNLOCKED", 16) == 0){
        last_op = unlockFile_op;
        free(response);
        elem_t * toDestroy = get_elem(locked_files, (char*)pathname, (int (*)(void *, void *)) strcmp);
        free(toDestroy);
        return 1;
    }//Se ho ricevuto dei file espulsi
    else {
        free(response); //TODO errno
        return -1;
    }

}

int sendToServer(void* content, size_t len, const char* contentType){

    if (writen(socket_fd, &len, sizeof(int)) < 0) {
        return -1;
    }
    if ( (writen(socket_fd, content, len) ) < 0 ){
        return -1;
    }

    return 0;
}

void* recieveFromServer(const char* contentType){

    size_t len = 0;

    //Ricevo lunghezza
    if ( readn(socket_fd, &len, sizeof(int)) <= 0){
        return NULL;
    }
    if( strcmp(contentType, "string") == 0) {
        void *response = malloc(sizeof(char) * len+1);
        if(response == NULL)
            return NULL;

        if ( readn(socket_fd, response, len+1) <= 0){
            return NULL;
        }

        return response;
    }

    else if( strcmp(contentType, "file") == 0){
        void *file_content = malloc(len);
        if(file_content == NULL)
            return NULL;

        if ( readn(socket_fd, file_content, len) <= 0){
            return NULL;
        }

        return file_content;
    }

    return NULL;
}


void reset(){

    connected = false;
    socket_fd = -1;
    strncpy(last_file_opened, "", MAX_FILENAME-1);
    strncpy(mySockname, "", MAX_SOCKNAME-1);
    last_op = closeConnection_op;
    if(locked_files != NULL) {
        list_destroy(locked_files, NULL);
        locked_files = NULL;
    }
}

