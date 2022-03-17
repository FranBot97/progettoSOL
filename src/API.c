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
#include <file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#define EISZERO "File is empty"

static bool connected = false;
static int socket_fd = -1;
char mySockname[MAX_SOCKNAME] = "";
char last_file_opened[MAX_FILENAME] = "";
list_t* locked_files = NULL; //Usare una richiesta per questo? Andare a cercare tutti i files?
//eventuale file espulso dalla openFile con flag O_CREATE che precede una writeFile
char expelled_file_name[MAX_FILENAME] = "";
void* expelled_file_content = NULL;
size_t expelled_file_size = 0;

enum  operations_ {
    NULL_op = -1,
    openConnection_op = 0,
    openFile_simple_op = 1,
    openFile_CREATE_op = 2,
    openFile_CREATE_LOCK_op = 3,
    openFile_LOCK_op = 4,
    closeConnection_op = 5,
    lockFile_op = 6,
    unlockFile_op = 7
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
    if(isConnected != 0) {
        printf("Connessione fallita, riprovare\n");
        return -1;
    }

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

    //Parsing nome del file
    char pathname_to_parse[MAX_ARGLEN];
    strcpy(pathname_to_parse, pathname);
    char filename[MAX_FILENAME];
    parseFilename(pathname_to_parse, filename);

    char* request;
    enum operations_ op = NULL_op;

    //Controllo se ho settato sia O_CREATE che O_LOCK
    if( (flags & O_CREATE) && (flags & O_LOCK) ){
        request = "OPEN_FILE_CREATE_LOCK";
        op = openFile_CREATE_LOCK_op;
    } //Altrimenti guardo se c'è solo O_CREATE
    else if(flags & O_CREATE){
        request = "OPEN_FILE_CREATE";
        op = openFile_CREATE_op;
        }
    //Altrimenti guardo se c'è solo O_LOCK
    else if(flags & O_LOCK){
        request = "OPEN_FILE_LOCK";
        op = openFile_LOCK_op;
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
    unsigned long len = strlen(filename);
    if (sendToServer((void*)filename, len, "string") != 0){
        return -1;
    }

    //Aspetto risposta
    char* response = recieveFromServer("string");
    if(response == NULL)
        return -1;

    //Se è stata una openFile con flag O_CREATE il server potrebbe aver espulso 1 file
    //per far posto a quello appena creato. Ne salvo il nome e il contenuto e la dimensione per
    //poterlo aggiungere alla lista dei file espulsi nel caso l'operazione successiva
    //fosse una writeFile( pathname , ... )
   if(strncmp(response, "EXPELLED", 8) == 0){

       //Leggo il nome del file espulso
        char* filename_expelled = recieveFromServer("string");
        strcpy(expelled_file_name, filename_expelled);
        //Leggo la dimensione del file espulso
        size_t file_len = 0;
        readn(socket_fd, &file_len, sizeof(unsigned long));
        void *file_content = NULL;
        if(file_len != 0) {
           //Leggo il contenuto del file espulso
           file_content = malloc(file_len);
       }
        expelled_file_size = file_len;

        if(!file_content && file_len != 0){
            free(filename_expelled);
            free(response);
            return -1;
        }
        if(file_len != 0)
            readn(socket_fd, file_content, file_len);

        expelled_file_content = file_content;
        printf("File espulso %s\n", filename_expelled);

        response = recieveFromServer("string");
        if(strcmp(response, "STOP") == 0){
            //OK - non necessario
        }
    }else{
       //Se non ci sono file espulsi pulisco le variabili
       strcpy(expelled_file_name, "");
       if(expelled_file_content) {
           free(expelled_file_content);
           expelled_file_content = NULL;
       }
       expelled_file_size = 0;
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

    if(!connected){
        errno = ENOTCONN;
        return -1;
    }

    if(!pathname){
        errno = EINVAL;
        return -1;
    }

    //Controllo se l'ultima operazione è stata una openFile con O_CREATE | O_LOCK su quel file
    if(last_op != openFile_CREATE_LOCK_op && (strcmp(last_file_opened, pathname)) == 0) {
        errno = ECANCELED;
        return -1;
    }
    printf("pathname %s\n", pathname);
    FILE* FileToWrite = fopen(pathname, "rb");
    if(!FileToWrite){
        errno = ENOENT;
        return -1;
    }
    char name[MAX_FILENAME];
    char pathname_to_Parse[MAX_FILENAME];
    strcpy(pathname_to_Parse, pathname);
    parseFilename(pathname_to_Parse, name);
    //Ora "name" contiene il nome del file senza tutto il path

    struct stat sb;
    if (stat(pathname, &sb) == -1){
        return -1;
    }
    off_t file_size =  sb.st_size;
    if(file_size == 0){
        printf("Il file %s e' vuoto", pathname);
        fclose(FileToWrite);
        return -1;
    }
    void* data = NULL;
    if(file_size != 0)
       data = malloc(file_size);

    while ( (!feof(FileToWrite)) && (file_size != 0)){
        fread(data, 1, file_size, FileToWrite);
    }
    //Invio l'operazione, il nome del file e il contenuto
    sendToServer("WRITE_FILE", strlen("WRITE_FILE")+1, "string");
    sendToServer((void*)name, strlen(name)+1, "string");
    writen(socket_fd, &file_size, sizeof(size_t));
    if(file_size != 0)
        writen(socket_fd, data, file_size);
    //Aspetto la risposta dal server
    char* response = recieveFromServer("string");

    //Prima di leggere la risposta controllo se ho un file espulso dalla openFile precedente
    if(dirname){
        //Controllo se la cartella esiste
        DIR* dir = opendir(dirname); //TODO check dopo

        if(expelled_file_content != NULL){
            if(dir){
                char newFilepath[MAX_ARGLEN];
                strcpy(newFilepath, dirname);
                strcat(newFilepath, "/");
                strcat(newFilepath, expelled_file_name);
                int name_len = strlen(expelled_file_name);
                char t = expelled_file_name[name_len-3];
                char x = expelled_file_name[name_len-2];
                char t2 = expelled_file_name[name_len-1];
                if(t == 't' && x == 'x' && t2 == 't'){
                   expelled_file_content = (char*)expelled_file_content;
                }
                FILE* expelled = fopen(newFilepath, "wb");
                if(expelled){
                    if (fwrite(expelled_file_content, expelled_file_size, 1, expelled) != expelled_file_size){
                        perror("Salvataggio del file espulso");
                    }
                }
            }
        }
        closedir(dir);
    }

    //Se ho uno o più file espulsi devo scriverli nella cartella se c'è
    while(strcmp(response, "EXPELLED") == 0){

        //Leggo il nome del file espulso
        char* filename = recieveFromServer("string");
        //Leggo la dimensione del file espulso
        size_t file_len = 0;
        void *file_content = NULL;
        readn(socket_fd, &file_len, sizeof(unsigned long));
        //Alloco lo spazio per il file
        if(file_len != 0) {
           file_content = malloc(file_len);
        }
        printf("size file to alloc: %ld", file_len);
        if(!file_content && file_len != 0){
            free(response);
            perror("Malloc");
            return -1;
        }
        //Leggo il contenuto del file espulso
        if(file_len != 0)
            readn(socket_fd, file_content, file_len);

        if(dirname){
            char newFilepath[MAX_ARGLEN];
            strcpy(newFilepath, dirname);
            strcat(newFilepath, "/");
            strcat(newFilepath, filename);
            FILE* expelled = fopen(newFilepath, "wb");
            if(expelled){
                if (fwrite(file_content, file_size, 1, expelled) != file_size){
                    perror("Salvataggio del file espulso");
                }
            }
            free(file_content);
        }

        //Leggo il prossimo messaggio
        response = recieveFromServer("string");
    }

    fclose(FileToWrite);
    free(data);

    if(strcmp(response, "OK") == 0){
        free(response);
        return 0;
    }else{

        free(response);
        return -1;
    }
}

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname){
    return 0;
}

int lockFile(const char* pathname){

    if(!connected){
        errno = ENOTCONN;
        return -1;
    }

    if(!pathname){
        errno = EINVAL;
        return -1;
    }

    printf("Lock file start\n");
    sendToServer("LOCK_FILE", strlen("LOCK_FILE")+1,"string");
    sendToServer((void*)pathname, strlen(pathname)+1, "string");

    char* response = recieveFromServer("string");
    if(response == NULL)
        return -1;

    if(strncmp(response, "OK", 2) == 0) {
        last_op = lockFile_op;
        add_head_element(locked_files, (char*)pathname);
        free(response);
        return 0;
    }else if(strcmp(response, "ALREADY LOCKED") == 0){
        last_op = lockFile_op;
        free(response);
        return 0;
    }else if(strcmp(response, "ERROR") == 0){
        free(response);
        return -1;
    }

    //TODO send request to server and if ok modify API list

    return -1;
}

int unlockFile(const char* pathname){

    if(!connected){
        errno = ENOTCONN;
        return -1;
    }

    if(!pathname){
        errno = EINVAL;
        return -1;
    }

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
    strcpy(expelled_file_name, "");
    expelled_file_size = 0;
    strncpy(mySockname, "", MAX_SOCKNAME-1);
    last_op = closeConnection_op;
    if(expelled_file_content != NULL) {
        free(expelled_file_content);
        expelled_file_content = NULL;
    }
    if(locked_files != NULL) {
        list_destroy(locked_files, NULL);
        locked_files = NULL;
    }

}

