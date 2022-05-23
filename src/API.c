#include <time.h>
#include <stdbool.h>
#include <list.h>
#include <unistd.h>
#include <sys/un.h>
#include <API.h>
#include <util.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/socket.h>
#include <request.h>

static bool connected = false;
static int socket_fd = -1;
bool print_info = false;
char mySockname[MAX_SOCKNAME] = "";
char lastOpenedFlags[MAX_FILENAME] =""; //Per la writeFile: ultimo file su cui è stata eseguita una open con O_CREATE | O_LOCK

int openConnection(const char* sockname, int msec, const struct timespec abstime){

    bool printed = false;

    if(connected) {
        errno = EISCONN;
        goto error;
    }

    if((sockname == NULL) || (msec < 0) ){
        errno = EINVAL;
        goto error;
    }

    if(strlen(sockname) > MAX_SOCKNAME){
        errno = ENAMETOOLONG;
        goto error;
    }

    socket_fd = socket(AF_UNIX,SOCK_STREAM,0); //sets errno
    if(socket_fd == -1) {
        goto error;
    }

    //Inizializzazione socket
    struct sockaddr_un serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, sockname, MAX_SOCKNAME-1);

    //prendo il tempo attuale
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);

    //Riprovo a connettermi finché non ho raggiunto "abstime"
    PRINT_INFO(print_info, "\n\n--- INFO OPERAZIONE --------------------------------------")
    printed = true;
    PRINT_INFO(print_info, "\nopenConnection: connessione al server in corso..")
    int isConnected = -1;
    while( (isConnected = connect(socket_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr) )) != 0
            && (now.tv_sec < abstime.tv_sec) ) {
        sleep(msec/1000); //Aspetto
        clock_gettime(CLOCK_REALTIME, &now); //Ricalcolo il tempo attuale
        PRINT_INFO(print_info, "\nopenConnection: tentativo di riconnessione..")
    }
    if(isConnected != 0) {
        goto error;
    }

    //Se va a buon fine inizializzo i parametri
    connected = true;
    strcpy(mySockname, sockname);
    PRINT_INFO(print_info, "\nopenConnection: connesso con successo alla socket %s", sockname)
    PRINT_INFO(print_info, "\n---------------------------------------------------------")
    return 0;

    error:
    if(!printed){
        PRINT_INFO(print_info, "\n\n--- INFO OPERAZIONE --------------------------------------")
    }
    PRINT_INFO(print_info, "\nopenConnection: impossibile connettersi alla socket %s. Info sull'errore: %s", sockname,
               strerror(errno))
    PRINT_INFO(print_info, "\n---------------------------------------------------------")
    return -1;

}

int closeConnection(const char* sockname){

    if(connected == false){
        errno = ENOTCONN;
        goto error;
    }

    if(strlen(sockname) > MAX_SOCKNAME){
        errno = ENAMETOOLONG;
        goto error;
    }

    if(sockname == NULL || strncmp(sockname, mySockname, MAX_FILENAME) != 0){
        errno = EINVAL;
        goto error;
    }

    if (close(socket_fd) != 0 ){
        goto error;
    }

    PRINT_INFO(print_info, "\n\n--- INFO OPERAZIONE --------------------------------------")
    PRINT_INFO(print_info, "\ncloseConnection: disconnessione dalla socket %s eseguita con successo", sockname)
    PRINT_INFO(print_info, "\n---------------------------------------------------------")
    return 0;

    error:
    PRINT_INFO(print_info, "\n\n--- INFO OPERAZIONE --------------------------------------")
    PRINT_INFO(print_info, "\ncloseConnection: impossibile disconnettersi dalla socket %s.\nInfo sull'errore: %s", sockname,
               strerror(errno))
    PRINT_INFO(print_info, "\n---------------------------------------------------------")
    return -1;
}


int openFile(const char* pathname, int flags){

    char err_arg[ERROR_STRLEN] = "";

    if(connected == false) {
        errno = ENOTCONN;
        goto error;
    }
    if(!pathname){
        errno = EINVAL;
        goto error;
    }
    if(strlen(pathname) > MAX_PATH){
        strcpy(err_arg, pathname);
        errno = ENAMETOOLONG;
        goto error;
    }

    //Preparazione stampa
    char flag_str[ERROR_STRLEN];
    if(flags == 0) strcpy(flag_str, "nessun flag");
    if(flags == 1) strcpy(flag_str, "il flag O_CREATE");
    if(flags == 2) strcpy(flag_str, "il flag O_LOCK");
    if(flags == 3) strcpy(flag_str, "i flag O_CREATE e O_LOCK");

    //Parsing nome del file
    char pathname_to_parse[MAX_ARGLEN];
    strcpy(pathname_to_parse, pathname);
    char filename[MAX_FILENAME];
    parseFilename(pathname_to_parse, filename);
    size_t filename_len = strlen(filename);
    if(filename_len > MAX_FILENAME){
        strcpy(err_arg, filename);
        errno = ENAMETOOLONG;
        goto error;
    }
    int opcode = OPEN;

    //Invio dati al server, nell'ordine: codice op, flags, lunghezza nome file, nome file
    if (writen(socket_fd, &opcode, sizeof(int)) <= 0)  goto error;
    if (writen(socket_fd, &flags, sizeof(int)) <= 0)  goto error;
    if (writen(socket_fd, &filename_len, sizeof(size_t)) <= 0)  goto error;
    if (writen(socket_fd, filename, filename_len) <= 0)  goto error;

    //Aspetto risposta
    int response = -1;
    int errno_copy = 0;
    if (readn(socket_fd, &response, sizeof(int)) <= 0) goto error;
    if (readn(socket_fd, &errno_copy, sizeof(int)) <= 0) goto error;

    if(response != 0){
        errno = errno_copy;
        goto error;
    }else //Se non ci sono stati errori
        if( flags == ( O_CREATE|O_LOCK )) strcpy(lastOpenedFlags, filename);

    errno = errno_copy;

    PRINT_INFO(print_info, "\n\n--- INFO OPERAZIONE --------------------------------------")
    PRINT_INFO(print_info, "\nopenFile: file %s aperto con successo sul server con %s",filename, flag_str);
    PRINT_INFO(print_info, "\n-----------------------------------------------------------")
    return 0;

    error:
    PRINT_INFO(print_info, "\n\n--- INFO OPERAZIONE --------------------------------------")
    PRINT_INFO(print_info, "\nopenFile: impossibile aprire il file %s sul server con %s.\nInfo sull'errore: %s %s", filename, flag_str, err_arg,
               strerror(errno))
    PRINT_INFO(print_info, "\n------------------------------------------------------------")
    return -1;
}

int closeFile(const char* pathname){

    char err_arg[ERROR_STRLEN] = "";

    if(connected == false) {
        errno = ENOTCONN;
        goto error;
    }
    if(!pathname){
        strcpy(err_arg, "arg 'pathname':");
        errno = EINVAL;
        goto error;
    }
    if(strlen(pathname) > MAX_PATH){
        strcpy(err_arg, "arg 'pathname':");
        errno = ENAMETOOLONG;
        goto error;
    }

    //Parsing nome del file
    char pathname_to_parse[MAX_ARGLEN];
    strcpy(pathname_to_parse, pathname);
    char filename[MAX_FILENAME];
    parseFilename(pathname_to_parse, filename);
    size_t filename_len = strlen(filename);
    if(filename_len > MAX_FILENAME){
        errno = ENAMETOOLONG;
        goto error;
    }
    if(filename_len <= 0){
        strcpy(err_arg, "nome del file:");
        errno = EINVAL;
        goto error;
    }
    int opcode = CLOSE;

    //Invio dati al server, nell'ordine: codice op, lunghezza nome file, nome file
    if (writen(socket_fd, &opcode, sizeof(int)) <= 0)   goto error;
    if (writen(socket_fd, &filename_len, sizeof(size_t)) <= 0)   goto error;
    if (writen(socket_fd, filename, filename_len) <= 0)  goto error;

    //Aspetto risposta
    int response = -1;
    int errno_copy = 0;
    if ( readn(socket_fd, &response, sizeof(int)) <= 0)  goto error;
    if (readn(socket_fd, &errno_copy, sizeof(int)) <= 0)  goto error;

    if(response != 0){
        errno = errno_copy;
        goto error;
    }else
        if(strcmp(lastOpenedFlags, filename) == 0) strcpy(lastOpenedFlags, "");

    errno = errno_copy;
    PRINT_INFO(print_info, "\n\n--- INFO OPERAZIONE --------------------------------------")
    PRINT_INFO(print_info, "\ncloseFile: file %s chiuso con successo sul server",filename)
    PRINT_INFO(print_info, "\n----------------------------------------------------------")
    return 0;

    error:
    PRINT_INFO(print_info, "\n\n--- INFO OPERAZIONE --------------------------------------")
    PRINT_INFO(print_info, "\ncloseFile: impossibile chiudere il file %s sul server.", filename)
    PRINT_INFO(print_info, "\n----------------------------------------------------------")

    return -1;
}

int writeFile(const char* pathname, const char* dirname){

    char err_arg[ERROR_STRLEN] = "";
    char ok_arg[ERROR_STRLEN] = "";
    DIR* dir = NULL;
    bool done = false;

    if(!connected){
        errno = ENOTCONN;
        goto error;
    }
    if(!pathname){
        strcpy(err_arg, "arg 'pathname':");
        errno = EINVAL;
        goto error;
    }
    if(strlen(pathname) > MAX_PATH ){
        strcpy(err_arg, "arg 'pathname':");
        errno = ENAMETOOLONG;
        goto error;
    }
    if(dirname != NULL && (strlen(dirname) > MAX_PATH)){
        strcpy(err_arg, "arg 'dirname':");
        errno = ENAMETOOLONG;
        goto error;
    }

    //Verifico se il file è presente nel dispositivo del client
    FILE* file = fopen(pathname, "rb");
    if(!file)
        goto error;

    //Leggo il contenuto del file dal dispositivo del client
    struct stat sb;
    if (stat(pathname, &sb) == -1) goto error;
    off_t file_size =  sb.st_size;
    if(file_size == 0){
        strcpy(err_arg, pathname);
        errno = ENODATA;
        fclose(file);
        goto error;
    }
    void* data = NULL;
    data = malloc(file_size);
    if(!data) goto error;
    while (!feof(file)) fread(data, 1, file_size, file);
    if(file) fclose(file);

    //Parsing nome del file
    char pathname_to_parse[MAX_PATH];
    strcpy(pathname_to_parse, pathname);
    char filename[MAX_FILENAME];
    parseFilename(pathname_to_parse, filename);
    size_t filename_len = strlen(filename);
    if(filename_len > MAX_FILENAME){
        strcpy(err_arg, "nome del file:");
        errno = ENAMETOOLONG;
        goto error;
    }
    if(filename_len <= 0){
        strcpy(err_arg, "nome del file:");
        errno = EINVAL;
        goto error;
    }
    if(strcmp(lastOpenedFlags, filename) != 0){
        //Controllo se è l'ultimo file su cui ho eseguito la open con entrambi i flag
        strcpy(err_arg, filename);
        strcat(err_arg, " non aperto sul server:");
        errno = EACCES;
        goto error;
    }

    int opcode = WRITE;
    //Invio dati al server, nell'ordine: codice op, lunghezza nome file, nome file, dimensione del file in bytes, contenuto
    if (writen(socket_fd, &opcode, sizeof(int)) <= 0) goto error;
    if (writen(socket_fd, &filename_len, sizeof(size_t)) <= 0) goto error;
    if (writen(socket_fd, filename, filename_len) <= 0) goto error;
    if (writen(socket_fd, &file_size, sizeof(off_t)) <= 0) goto error;
    if (writen(socket_fd, data, file_size) <= 0) goto error;
    //Aspetto responso per procedere
    int response; int errno_copy;
    if (readn(socket_fd, &response, sizeof(int)) <= 0) goto error;
    if (readn(socket_fd, &errno_copy, sizeof(int)) <= 0) goto error;
    if(response != 0){
        free(data);
        errno = errno_copy;
        goto error;
    }
    free(data);
    done = true;

    //Aspetto eventuali file espulsi: leggo la dimensione del prossimo file espulso, quando ricevo 0 mi fermo
    size_t storedFile_len = 0;
    size_t storedFile_name_len = 0;
    unsigned long received_bytes = 0;
    unsigned long stored_bytes = 0;
    char storedFile_name[MAX_FILENAME] = "";
    void* storedFile_data = NULL;
    if(dirname != NULL) {
        dir = opendir(dirname);
        if(!dir) {
            strcpy(err_arg, dirname);
            goto error;
        }
    }
    do{
        if (readn(socket_fd, &storedFile_len, sizeof(size_t)) <= 0) goto error;
        if(storedFile_len == 0) break; //Quando ricevo 0 esco
        storedFile_data = malloc(storedFile_len);
        if(!storedFile_data) goto error;

        //Ricevo i restanti dati del file, nell'ordine: lunghezza nome file, nome file, contenuto
        if (readn(socket_fd, &storedFile_name_len, sizeof(size_t)) <= 0) goto error;
        if (readn(socket_fd, storedFile_name, storedFile_name_len) <= 0) goto error;
        if (readn(socket_fd, storedFile_data, storedFile_len) <= 0) goto error;
        storedFile_name[storedFile_name_len] = '\0';
        received_bytes+=storedFile_len;
        //Salvo il file nella cartella SOLO SE specificata e già esistente nel dispositivo
        if(dirname != NULL && dir ) {
            char complete_path[MAX_PATH + MAX_FILENAME] = "";
            strcpy(complete_path, dirname);
            if (complete_path[strlen(complete_path)-1] != '/')
                strcat(complete_path, "/");
            strcat(complete_path, storedFile_name);
            //Creo e scrivo il file ricevuto
            FILE* storedFile = fopen(complete_path, "wb");
            if(!storedFile){
                strcpy(err_arg, "apertura in locale del file ");
                strcat(err_arg, complete_path);
                goto error;
            }
            strcat(ok_arg, storedFile_name); strcat(ok_arg, "\n");
            if (fwrite(storedFile_data, 1, storedFile_len, storedFile) != storedFile_len){
                strcpy(err_arg, "scrittura in locale nel file ");
                strcat(err_arg, complete_path);
                goto error;
            }
            stored_bytes+=storedFile_len;
            if(storedFile) fclose(storedFile);
        }
        if(storedFile_data) free(storedFile_data);
        storedFile_data = NULL;
    }while(true);
    PRINT_INFO(print_info, "\n\n--- INFO OPERAZIONE --------------------------------------")
    PRINT_INFO(print_info, "\nwriteFile: file %s scritto correttamente sul server.\n"
                           "Scritti %ld bytes sul server,"
                           "ricevuti %ld bytes e salvati %ld bytes.",
                           pathname,file_size,received_bytes,stored_bytes)
    if(dir){
        PRINT_INFO(print_info, "\nFile memorizzati nella cartella %s: \n%s", dirname, ok_arg)
    }
    PRINT_INFO(print_info, "\n-----------------------------------------------------------")
    if(storedFile_data) free(storedFile_data);
    if(dir) closedir(dir);
    if(storedFile_data) free(storedFile_data);
    return 0;

    error:
    PRINT_INFO(print_info, "\n\n--- INFO OPERAZIONE --------------------------------------")
    PRINT_INFO(print_info, "\nwriteFile: errore durante l'operazione di scrittura del file %s.\nInfo sull'errore: %s %s\n", pathname, err_arg,strerror(errno))
    if(done){  PRINT_INFO(print_info, "Scritti %ld bytes sul server,", file_size)}
    if(done) {PRINT_INFO(print_info, "ricevuti %ld bytes e salvati %ld bytes. ", received_bytes, stored_bytes)}
    if(dir && (strcmp(ok_arg, "") != 0)) {
        PRINT_INFO(print_info, "\nFile memorizzati nella cartella %s:\n%s",
                                dirname, ok_arg)
    }
    PRINT_INFO(print_info, "\n-----------------------------------------------------------")
    return -1;
}

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname){

    char err_arg[ERROR_STRLEN] = "";
    char ok_arg[ERROR_STRLEN] = "";
    DIR *dir = NULL;
    bool done = false;

    if(!connected){
        errno = ENOTCONN;
        goto error;
    }

    if(!pathname || !buf){
        strcpy(err_arg, "arg 'pathname':");
        errno = EINVAL;
        goto error;
    }

    if(size <= 0){
        strcpy(err_arg, "arg 'size':");
        errno = ENODATA;
        goto error;
    }

    if(strlen(pathname) > MAX_PATH ){
        strcpy(err_arg, "arg 'pathname':");
        errno = ENAMETOOLONG;
        goto error;
    }
    if(dirname != NULL && (strlen(dirname) > MAX_PATH)){
        strcpy(err_arg, "arg 'dirname':");
        errno = ENAMETOOLONG;
        goto error;
    }

    //Parsing nome del file
    char pathname_to_parse[MAX_PATH];
    strcpy(pathname_to_parse, pathname);
    char filename[MAX_FILENAME];
    parseFilename(pathname_to_parse, filename);
    size_t filename_len = strlen(filename);
    if(filename_len > MAX_FILENAME){
        strcpy(err_arg, "nome del file:");
        errno = ENAMETOOLONG;
        return -1;
    }
    if(filename_len <= 0){
        strcpy(err_arg, "nome del file:");
        errno = EINVAL;
        return -1;
    }

    int opcode = APPEND;
    //Invio dati al server, nell'ordine: codice op, lunghezza nome file, nome file, dimensione del contenuto in bytes, contenuto
    if (writen(socket_fd, &opcode, sizeof(int)) <= 0) goto error;
    if (writen(socket_fd, &filename_len, sizeof(size_t)) <= 0) goto error;
    if (writen(socket_fd, filename, filename_len) <= 0) goto error;
    if (writen(socket_fd, &size, sizeof(size_t)) <= 0) goto error;
    if (writen(socket_fd, buf, size) <= 0) goto error;

    //Aspetto responso per procedere
    int response; int errno_copy;
    if (readn(socket_fd, &response, sizeof(int)) <= 0) goto error;
    if (readn(socket_fd, &errno_copy, sizeof(int)) <= 0) goto error;
    if(response != 0){
        errno = errno_copy;
        goto error;
    }
    done = true;

    //Aspetto eventuali file espulsi: leggo la dimensione del prossimo file espulso, quando ricevo 0 mi fermo
    size_t storedFile_len = 0;
    size_t storedFile_name_len = 0;
    unsigned long received_bytes = 0;
    unsigned long stored_bytes = 0;
    char storedFile_name[MAX_FILENAME] = "";
    void* storedFile_data = NULL;
    if(dirname) {
        dir = opendir(dirname);
        if(!dir) {
            strcpy(err_arg, dirname);
            goto error;
        }
    }
    do{
        if (readn(socket_fd, &storedFile_len, sizeof(size_t)) <= 0) goto error;
        if(storedFile_len == 0) break; //Quando ricevo 0 esco
        storedFile_data = malloc(storedFile_len);
        if(!storedFile_data) goto error;

        //Ricevo i restanti dati del file, nell'ordine: lunghezza nome file, nome file, contenuto
        if (readn(socket_fd, &storedFile_name_len, sizeof(size_t)) <= 0) goto error;
        if (readn(socket_fd, storedFile_name, storedFile_name_len) <= 0) goto error;
        if (readn(socket_fd, storedFile_data, storedFile_len) <= 0) goto error;
        storedFile_name[storedFile_name_len] = '\0';
        received_bytes+=storedFile_len;
        //Salvo il file nella cartella SOLO SE specificata e già esistente nel dispositivo
        if(dirname != NULL && dir ) {
            char complete_path[MAX_PATH + MAX_FILENAME] = "";
            strcpy(complete_path, dirname);
            if (complete_path[strlen(complete_path)-1] != '/') strcat(complete_path, "/");
            strcat(complete_path, storedFile_name);
            //Creo e scrivo il file ricevuto
            FILE* storedFile = fopen(complete_path, "wb");
            if(!storedFile){
                strcpy(err_arg, "apertura in locale del file ");
                strcat(err_arg, complete_path);
                goto error;
            }
            strcat(ok_arg, storedFile_name); strcat(ok_arg, "\n");
            if (fwrite(storedFile_data, 1, storedFile_len, storedFile) != storedFile_len){
                strcpy(err_arg, "scrittura in locale nel file ");
                strcat(err_arg, complete_path);
                goto error;
            }
            stored_bytes+=storedFile_len;
            if(storedFile) fclose(storedFile);
        }
        if(storedFile_data) free(storedFile_data);
        storedFile_data = NULL;
    }while(true);

    PRINT_INFO(print_info, "\n\n--- INFO OPERAZIONE --------------------------------------")
    PRINT_INFO(print_info, "\nappendToFile: %ld byte aggiunti al file %s sul server.\n"
                           "Ricevuti %ld bytes e salvati %ld bytes.",
               size,pathname,received_bytes,stored_bytes)
    if(dir){
        PRINT_INFO(print_info, "\nFile memorizzati nella cartella %s:\n%s", dirname, ok_arg)
    }
    PRINT_INFO(print_info, "\n----------------------------------------------------------")

    if(storedFile_data) free(storedFile_data);
    if(dir) closedir(dir);
    if(storedFile_data) free(storedFile_data);
    return 0;

    error:
    PRINT_INFO(print_info, "\n\n--- INFO OPERAZIONE --------------------------------------")
    PRINT_INFO(print_info, "\nappendToFile: errore durante l'operazione di scrittura in append al file %s.\nInfo sull'errore: %s %s\n", pathname, err_arg,
               strerror(errno))
    if(done)  {PRINT_INFO(print_info, "Scritti %ld bytes sul server", size)}
    if(done) {PRINT_INFO(print_info, ", ricevuti %ld bytes e salvati %ld bytes. \n", received_bytes, stored_bytes)}
    if(dir && (strcmp(ok_arg, "") != 0)) {
        PRINT_INFO(print_info, "\nFile memorizzati nella cartella %s:\n%s",
                                dirname, ok_arg)
    }
    PRINT_INFO(print_info, "\n----------------------------------------------------------")
    return -1;
}

int readFile(const char* pathname, void** buf, size_t* size){
    char err_arg[ERROR_STRLEN] = "";
    //int errno_copy = 0;

    if(!connected){
        errno = ENOTCONN;
        goto error;
    }

    if(!pathname){
        strcpy(err_arg, "arg 'pathname'");
        errno = EINVAL;
        goto error;
    }

    //Parsing nome del file
    char pathname_to_parse[MAX_PATH];
    strcpy(pathname_to_parse, pathname);
    char filename[MAX_FILENAME];
    parseFilename(pathname_to_parse, filename);
    size_t filename_len = strlen(filename);
    if(filename_len > MAX_FILENAME){
        strcpy(err_arg, "nome del file:");
        errno = ENAMETOOLONG;
        goto error;
    }
    if(filename_len <= 0){
        strcpy(err_arg, "nome del file:");
        errno = EINVAL;
        goto error;
    }

    int opcode = READ;

    //Invio dati al server, nell'ordine: codice op, lunghezza nome file, nome file, dimensione del file in bytes, contenuto
    if (writen(socket_fd, &opcode, sizeof(int)) <= 0)  goto error;
    if (writen(socket_fd, &filename_len, sizeof(size_t)) <= 0)  goto error;
    if (writen(socket_fd, filename, filename_len) <= 0)  goto error;
    filename[filename_len] = '\0';

    //Aspetto il responso, nell'ordine: responso, dimensione del file letto e contenuto
    int response = 0;
    if (readn(socket_fd, &response, sizeof(int)) <= 0)  goto error;
    if(response == 0){
        //leggo errno e esco
        if (readn(socket_fd, &errno, sizeof(int)) <= 0)  goto error;
        goto error;
    }
    //Leggo dimensione del file letto
    if (readn(socket_fd, size, sizeof(size_t)) <= 0)  goto error;
    *buf = malloc(*size); if(!buf)  goto error;
    if (readn(socket_fd, *buf, *size) <= 0) return -1;
    PRINT_INFO(print_info, "\n\n--- INFO OPERAZIONE --------------------------------------")
    PRINT_INFO(print_info, "\nreadFile: file %s letto correttamente dal server, %ld bytes memorizzati nel parametro 'buf'", filename, *size)
    PRINT_INFO(print_info, "\n----------------------------------------------------------")
    return 0;

    error:
    PRINT_INFO(print_info, "\n\n--- INFO OPERAZIONE --------------------------------------")
    PRINT_INFO(print_info, "\nreadFile: errore durante la lettura del file %s dal server.\nInfo sull'errore: %s %s", filename, err_arg, strerror(errno))
    PRINT_INFO(print_info, "\n----------------------------------------------------------")
    return -1;
}

int readNFiles(int N, const char* dirname){

    char err_arg[ERROR_STRLEN] = "";
    char ok_arg[ERROR_STRLEN] = "";
    DIR *dir = NULL;

    if(!connected){
        errno = ENOTCONN;
        goto error;
    }

   if( dirname && strlen(dirname) > MAX_PATH){
       strcpy(err_arg, "nome della cartella");
        errno = EINVAL;
        goto error;
    }

    char complete_path[MAX_PATH*2] = "";
    if(dirname) {
      dir = opendir(dirname);
        if(!dir) {
            strcpy(err_arg, dirname);
            goto error;
        }
      strcpy(complete_path, dirname);
      if (dirname[strlen(dirname) - 1] != '/')
          strcat(complete_path, "/");
   }

    int opcode = READN;
    int conta_file = 0;
    //Invio al server il codice operazione e il numero di file che voglio leggere
    if(writen(socket_fd, &opcode, sizeof(int)) <= 0) goto error;
    if(writen(socket_fd, &N, sizeof(int)) <= 0) goto error;
    //Attendo di leggere nell'ordine: dimensione file, contenuto, lunghezza nome file, nome file
    size_t file_size = 0;
    void* file_data = NULL;
    size_t file_name_len = 0;
    unsigned long received_bytes = 0;
    unsigned long stored_bytes = 0;
    char filename[MAX_FILENAME] = "";
    char complete_path_old[MAX_PATH*2];
    strcpy(complete_path_old, complete_path);
    //quando ricevo una dimensione non valida (<=0) termino
    while(true){
        if(readn(socket_fd, &file_size, sizeof(size_t)) <= 0) goto error;
        if(file_size <= 0) break;
        file_data = malloc(file_size);
        if(!file_data)
            goto error;

        if(readn(socket_fd, file_data, file_size) <= 0) goto error;
        if(readn(socket_fd, &file_name_len, sizeof(size_t)) <= 0) goto error;
        if(readn(socket_fd, filename, file_name_len) <= 0) goto error;
        filename[file_name_len] = '\0';
        received_bytes+=file_size;
        if(dir) {
            //Adesso ho tutti i dati che mi servono per salvare il file, creo il percorso
            strcpy(complete_path, complete_path_old);
            strncat(complete_path, filename, strlen(filename)+1);
            FILE *file = fopen(complete_path, "wb");
            if (!file) {
                strcpy(err_arg, "apertura del file ");
                strcat(err_arg, filename);
                goto error;
            } else {
                //scrivo il contenuto nel file
                if (fwrite(file_data, 1, file_size, file) != file_size) {
                    strcpy(err_arg, "scrittura nel file ");
                    strcat(err_arg, filename);
                    goto error;
                }
                conta_file++;
                if (fclose(file) != 0) {
                    strcpy(err_arg, "chiusura del file ");
                    strcat(err_arg, filename);
                    goto error;
                }
                stored_bytes+=file_size;
                strcat(ok_arg, filename);
                strcat(ok_arg, "\n");
            }
        }
        //Pulisco risorse
        strcpy(filename, "");
        file_size = 0;
        file_name_len = 0;
        free(file_data);
        file_data = NULL;
    }
    if(dir)
        if (closedir(dir) != 0) {
            strcpy(err_arg, "chiusura della cartella ");
            strcat(err_arg, dirname);
            goto error;
        };
    //Quando ricevo 0 leggo result e errno per controllare eventuali errori
    int errno_copy;
    int result;
    if(readn(socket_fd, &result, sizeof(int)) <= 0) goto error;
    if(readn(socket_fd, &errno_copy, sizeof(int)) <= 0) goto error;
    errno = errno_copy;

    if(errno_copy == 0) { //Se errno == 0 conta_file è valido
        PRINT_INFO(print_info, "\n\n--- INFO OPERAZIONE --------------------------------------")
        PRINT_INFO(print_info, "\nreadNFiles: lettura di %d file dal server effettuata.\n"
                               "% ld bytes ricevuti, %ld bytes salvati.", result, received_bytes, stored_bytes)
        if(dir) PRINT_INFO(print_info, "\nFile memorizzati nella cartella %s:\n%s", dirname, ok_arg);
        PRINT_INFO(print_info, "\n----------------------------------------------------------")
        return conta_file;
    }

    error:
    PRINT_INFO(print_info, "\n\n--- INFO OPERAZIONE --------------------------------------")
    PRINT_INFO(print_info, "\nreadNFiles: errore nella lettura di file dal server.\n"
                           "% ld bytes ricevuti, %ld bytes salvati.\nInfo sull'errore: %s %s", received_bytes, stored_bytes, err_arg,
               strerror(errno))
    if(dir && (strcmp(ok_arg, "") != 0)) PRINT_INFO(print_info, "\nFile memorizzati nella cartella %s:\n%s", dirname, ok_arg)
    PRINT_INFO(print_info, "\n----------------------------------------------------------")
    return -1;
}

int lockFile(const char* pathname){

    char str_arg[ERROR_STRLEN] = "";

    if(connected == false) {
        errno = ENOTCONN;
        goto error;
    }
    if(!pathname){
        strcpy(str_arg, "arg 'pathname'");
        errno = EINVAL;
        goto error;
    }
    if(strlen(pathname) > MAX_PATH){
        strcpy(str_arg, "arg 'pathname'");
        errno = ENAMETOOLONG;
        goto error;
    }

    //Parsing nome del file
    char pathname_to_parse[MAX_ARGLEN];
    strcpy(pathname_to_parse, pathname);
    char filename[MAX_FILENAME];
    parseFilename(pathname_to_parse, filename);
    size_t filename_len = strlen(filename);
    if(filename_len > MAX_FILENAME){
        strcpy(str_arg, "nome del file");
        errno = ENAMETOOLONG;
        goto error;
    }
    if(filename_len <= 0){
        strcpy(str_arg, "nome del file");
        errno = EINVAL;
        goto error;
    }
    int opcode = LOCK;
    int errno_copy = 0;
    int response = -1;
    int try = 0;
    PRINT_INFO(print_info, "\n\n--- INFO OPERAZIONE --------------------------------------")
    do{
        if(try > 0){
            PRINT_INFO(print_info, "\nlockFile: un altro utente ha acquisito la mutua esclusione sul file %s, in attesa che venga rilasciata.. ", filename)
        }
        //Invio dati al server, nell'ordine: codice op, lunghezza nome file, nome file
        if (writen(socket_fd, &opcode, sizeof(int)) <= 0) goto error;
        if (writen(socket_fd, &filename_len, sizeof(size_t)) <= 0) goto error;
        if (writen(socket_fd, filename, filename_len) <= 0) goto error;

        //Aspetto risposta
        if (readn(socket_fd, &response, sizeof(int)) <= 0) goto error;
        if (readn(socket_fd, &errno_copy, sizeof(int)) <= 0) goto error;

        try++;

        //Se è andato a buon fine ok
        if (response == 0)
            break;

    }while(errno_copy == EPERM);
    errno = errno_copy;
    if(response == 0){
        PRINT_INFO(print_info, "\nlockFile: la mutua esclusione sul file %s è stata acquisita correttamente sul server", filename)
        PRINT_INFO(print_info, "\n----------------------------------------------------------")
        return response;
    }
    error:
        PRINT_INFO(print_info, "\nlockFile: la mutua esclusione sul file %s non può essere acquisita "
                               "\nInfo sull'errore: %s %s", filename, str_arg, strerror(errno))
    PRINT_INFO(print_info, "\n----------------------------------------------------------")
    return -1;
}

int unlockFile(const char* pathname) {

    char str_arg[ERROR_STRLEN] = "";

    if(connected == false) {
        errno = ENOTCONN;
        goto error;
    }
    if(!pathname){
        strcpy(str_arg, "arg 'pathname'");
        errno = EINVAL;
        goto error;
    }
    if(strlen(pathname) > MAX_PATH){
        strcpy(str_arg, "arg 'pathname'");
        errno = ENAMETOOLONG;
        goto error;
    }

    //Parsing nome del file
    char pathname_to_parse[MAX_ARGLEN];
    strcpy(pathname_to_parse, pathname);
    char filename[MAX_FILENAME];
    parseFilename(pathname_to_parse, filename);
    size_t filename_len = strlen(filename);
    if(filename_len > MAX_FILENAME){
        strcpy(str_arg, "nome del file");
        errno = ENAMETOOLONG;
        goto error;
    }
    if(filename_len <= 0){
        strcpy(str_arg, "nome del file");
        errno = EINVAL;
        goto error;
    }
    int opcode = UNLOCK;
    int errno_copy;
    int response = -1;

    //Invio dati al server, nell'ordine: codice op, lunghezza nome file, nome file
    if (writen(socket_fd, &opcode, sizeof(int)) <= 0)  goto error;
    if (writen(socket_fd, &filename_len, sizeof(size_t)) <= 0)  goto error;
    if (writen(socket_fd, filename, filename_len) <= 0)  goto error;

    //Aspetto risposta
    if (readn(socket_fd, &response, sizeof(int)) <= 0)  goto error;
    if (readn(socket_fd, &errno_copy, sizeof(int)) <= 0)  goto error;
    errno = errno_copy;

    //Se è andato a buon fine ok
    if (response == 0) {
        if (strcmp(lastOpenedFlags, filename) == 0) strcpy(lastOpenedFlags, "");
        PRINT_INFO(print_info, "\n\n--- INFO OPERAZIONE --------------------------------------")
        PRINT_INFO(print_info, "\nunlockFile: la mutua esclusione sul file %s è stata rilasciata correttamente sul server ", filename)
        PRINT_INFO(print_info, "\n----------------------------------------------------------")
        return 0;
    }

    error:
    PRINT_INFO(print_info, "\n\n--- INFO OPERAZIONE --------------------------------------")
    PRINT_INFO(print_info, "\nunlockFile: la mutua esclusione sul file %s non può essere rilasciata "
                           "\nInfo sull'errore: %s %s", filename, str_arg, strerror(errno))
    PRINT_INFO(print_info, "\n----------------------------------------------------------")
    return -1;
}

int removeFile(const char* pathname){

    char err_arg[ERROR_STRLEN] = "";

    if(connected == false) {
        errno = ENOTCONN;
        goto error;
    }
    if(!pathname){
        strcpy(err_arg, "arg 'pathname'");
        errno = EINVAL;
        goto error;
    }
    if(strlen(pathname) > MAX_PATH){
        strcpy(err_arg, "arg 'pathname'");
        errno = ENAMETOOLONG;
        goto error;
    }

    //Parsing nome del file
    char pathname_to_parse[MAX_ARGLEN];
    strcpy(pathname_to_parse, pathname);
    char filename[MAX_FILENAME];
    parseFilename(pathname_to_parse, filename);
    size_t filename_len = strlen(filename);
    if(filename_len > MAX_FILENAME){
        strcpy(err_arg, "nome del file");
        errno = ENAMETOOLONG;
        goto error;
    }
    if(filename_len <= 0){
        strcpy(err_arg, "nome del file");
        errno = EINVAL;
        goto error;
    }
    int opcode = REMOVE;
    int errno_copy;
    int response = -1;

    //Invio dati al server, nell'ordine: codice op, lunghezza nome file, nome file
    if (writen(socket_fd, &opcode, sizeof(int)) <= 0) goto error;
    if (writen(socket_fd, &filename_len, sizeof(size_t)) <= 0) goto error;
    if (writen(socket_fd, filename, filename_len) <= 0) goto error;

    //Aspetto risposta
    if (readn(socket_fd, &response, sizeof(int)) <= 0) goto error;
    if (readn(socket_fd, &errno_copy, sizeof(int)) <= 0) goto error;
    errno = errno_copy;

    //Se è andato a buon fine ok
    if (response == 0) {
        if(strcmp(lastOpenedFlags, filename) == 0) strcpy(lastOpenedFlags, "");
        PRINT_INFO(print_info, "\n\n--- INFO OPERAZIONE --------------------------------------")
        PRINT_INFO(print_info, "\nremoveFile: la cancellazione del file %s dal server è andata a buon fine.", filename)
        PRINT_INFO(print_info, "\n----------------------------------------------------------")
        return 0;
    }

    error:
    PRINT_INFO(print_info, "\n\n--- INFO OPERAZIONE --------------------------------------")
    PRINT_INFO(print_info, "\nremoveFile: la cancellazione del file %s dal server è fallita. "
                           "\nInfo sull'errore: %s %s", filename, err_arg, strerror(errno))
    PRINT_INFO(print_info, "\n----------------------------------------------------------")

    return -1;

}





