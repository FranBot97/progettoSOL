//TODO controlla strncpy strlen ecc.. e gli invii delle stringhe
//TODO decidere se fare perror dentro API o fuori (meglio fuori per coerenza)
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
char mySockname[MAX_SOCKNAME] = "";
char lastOpenedFlags[MAX_FILENAME] =""; //Per la writeFile: ultimo file su cui è stata eseguita una open con O_CREATE | O_LOCK

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
        if(errno != 0)
            perror("Open Connection");
        return -1;
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
        errno = isConnected;
        perror("Open Connection");
        return -1;
    }

    //Se va a buon fine inizializzo i parametri
    connected = true;
    printf("Connesso con successo\n");
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

    if (close(socket_fd) != 0 ){
        perror("Close connection");
        return -1;
    }

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
    if(strlen(pathname) > MAX_PATH){
        errno = ENAMETOOLONG;
        return -1;
    }

    //Parsing nome del file
    char pathname_to_parse[MAX_ARGLEN];
    strcpy(pathname_to_parse, pathname);
    char filename[MAX_FILENAME];
    parseFilename(pathname_to_parse, filename);
    size_t filename_len = strlen(filename);
    if(filename_len > MAX_FILENAME){
        errno = ENAMETOOLONG;
        return -1;
    }
    int opcode = OPEN;

    //Invio dati al server, nell'ordine: codice op, flags, lunghezza nome file, nome file
    if (writen(socket_fd, &opcode, sizeof(int)) <= 0) return -1;
    if (writen(socket_fd, &flags, sizeof(int)) <= 0) return -1;
    if (writen(socket_fd, &filename_len, sizeof(size_t)) <= 0) return -1;
    if (writen(socket_fd, filename, filename_len) <= 0) return -1;

    //Aspetto risposta
    int response = -1;
    if ( readn(socket_fd, &response, sizeof(int)) <= 0) return -1;
    if (readn(socket_fd, &errno, sizeof(int)) <= 0) return -1;

    if(response != 0)
        perror("Openfile");
    else //Se non ci sono stati errori
        if( flags == ( O_CREATE|O_LOCK )) strcpy(lastOpenedFlags, filename);

    return response;

    //TODO
    //Se è stata una server_openFile con flag O_CREATE il server potrebbe aver espulso 1 file
    //per far posto a quello appena creato. Ne salvo il nome e il contenuto e la dimensione per
    //poterlo aggiungere alla lista dei file espulsi nel caso l'operazione successiva
    //fosse una writeFile( pathname , ... )

}

int closeFile(const char* pathname){

    if(connected == false) {
        errno = ENOTCONN;
        return -1;
    }
    if(!pathname){
        errno = EINVAL;
        return -1;
    }
    if(strlen(pathname) > MAX_PATH){
        errno = ENAMETOOLONG;
        return -1;
    }

    //Parsing nome del file
    char pathname_to_parse[MAX_ARGLEN];
    strcpy(pathname_to_parse, pathname);
    char filename[MAX_FILENAME];
    parseFilename(pathname_to_parse, filename);
    size_t filename_len = strlen(filename);
    if(filename_len > MAX_FILENAME){
        errno = ENAMETOOLONG;
        return -1;
    }
    if(filename_len <= 0){
        errno = EINVAL;
        return -1;
    }
    int opcode = CLOSE;

    //Invio dati al server, nell'ordine: codice op, lunghezza nome file, nome file
    if (writen(socket_fd, &opcode, sizeof(int)) <= 0) return -1;
    if (writen(socket_fd, &filename_len, sizeof(size_t)) <= 0) return -1;
    if (writen(socket_fd, filename, filename_len) <= 0) return -1;

    //Aspetto risposta
    int response = -1;
    if ( readn(socket_fd, &response, sizeof(int)) <= 0) return -1;
    if (readn(socket_fd, &errno, sizeof(int)) <= 0) return -1;

    if(response != 0)
        perror("Close file");
    else
        if(strcmp(lastOpenedFlags, filename) == 0) strcpy(lastOpenedFlags, "");

    return response;

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
    if(strlen(pathname) > MAX_PATH || (dirname != NULL && strlen(dirname) > MAX_PATH)){
        errno = ENAMETOOLONG;
        return -1;
    }

    //Verifico se il file è presente nel dispositivo del client
    FILE* file = fopen(pathname, "rb");
    if(!file){
        perror("Write file - lato client");
        return -1;
    }

    //Leggo il contenuto del file dal dispositivo del client
    struct stat sb;
    if (stat(pathname, &sb) == -1) return -1;
    off_t file_size =  sb.st_size;
    if(file_size == 0){
        errno = ENODATA;
        fclose(file);
        return -1;
    }
    void* data = NULL;
    data = malloc(file_size);
    if(!data) return -1;
    while (!feof(file)) fread(data, 1, file_size, file);

    //Parsing nome del file
    char pathname_to_parse[MAX_PATH];
    strcpy(pathname_to_parse, pathname);
    char filename[MAX_FILENAME];
    parseFilename(pathname_to_parse, filename);
    size_t filename_len = strlen(filename);
    if(filename_len > MAX_FILENAME){
        errno = ENAMETOOLONG;
        return -1;
    }
    if(filename_len <= 0){
        errno = EINVAL;
        return -1;
    }
    if(strcmp(lastOpenedFlags, filename) != 0){
        //Controllo se è l'ultimo file su cui ho eseguito la open con entrambi i flag
        errno = EPERM;
        return -1;
    }

    int opcode = WRITE;
    printf("%zu,%s,%ld ", filename_len, filename, file_size);
    //Invio dati al server, nell'ordine: codice op, lunghezza nome file, nome file, dimensione del file in bytes, contenuto
    if (writen(socket_fd, &opcode, sizeof(int)) <= 0) return -1;
    if (writen(socket_fd, &filename_len, sizeof(size_t)) <= 0) return -1;
    if (writen(socket_fd, filename, filename_len) <= 0) return -1;
    if (writen(socket_fd, &file_size, sizeof(off_t)) <= 0) return -1;
    if (writen(socket_fd, data, file_size) <= 0) return -1;

    //Aspetto responso per procedere
    int response; int errno_copy;
    if (readn(socket_fd, &response, sizeof(int)) <= 0) return -1;
    if (readn(socket_fd, &errno_copy, sizeof(int)) <= 0) return -1;
    if(response != 0){
        free(data);
        errno = errno_copy;
        perror("Write file");
        return -1;
    }

    //Aspetto eventuali file espulsi: leggo la dimensione del prossimo file espulso, quando ricevo 0 mi fermo
    size_t storedFile_len = 0;
    size_t storedFile_name_len = 0;
    char storedFile_name[MAX_FILENAME] = "";
    void* storedFile_data = NULL;
    DIR* dir = NULL;
    if(dirname) dir = opendir(dirname);
    if(!dir) perror("Write file - percorso cartella associata");
    do{
        if (readn(socket_fd, &storedFile_len, sizeof(size_t)) <= 0) return -1;
        if(storedFile_len == 0) break; //Quando ricevo 0 esco
        storedFile_data = malloc(storedFile_len);

        //Ricevo i restanti dati del file, nell'ordine: lunghezza nome file, nome file, contenuto
        if (readn(socket_fd, &storedFile_name_len, sizeof(size_t)) <= 0) return -1;
        if (readn(socket_fd, storedFile_name, storedFile_name_len) <= 0) return -1;
        if (readn(socket_fd, storedFile_data, storedFile_len) <= 0) return -1;

        //Salvo il file nella cartella SOLO SE specificata e già esistente nel dispositivo
        if(dirname != NULL && dir ) {
            char complete_path[MAX_PATH + MAX_FILENAME] = "";
            strcpy(complete_path, dirname);
            if (complete_path[strlen(complete_path)-1] != '/') strcat(complete_path, "/");
            strcat(complete_path, storedFile_name);
            //Creo e scrivo il file ricevuto
            FILE* storedFile = fopen(complete_path, "wb");
            if(!storedFile){
                perror("Write file - salvataggio del file espulso");
                return -1;
            }
            if (fwrite(storedFile_data, 1, storedFile_len, storedFile) != storedFile_len){
                perror("Write file - scrittura contenuto file espulso");
                return -1;
            }
            if(storedFile) fclose(storedFile);
        }
        if(storedFile_data) free(storedFile_data);
        storedFile_data = NULL;
    }while(true);

    if(storedFile_data) free(storedFile_data);
    if(dir) closedir(dir);
    if(storedFile_data) free(storedFile_data);
    return 0;
}

int readFile(const char* pathname, void** buf, size_t* size){

    if(!pathname){
        errno = EINVAL;
        return -1;
    }

    //Parsing nome del file
    char pathname_to_parse[MAX_PATH];
    strcpy(pathname_to_parse, pathname);
    char filename[MAX_FILENAME];
    parseFilename(pathname_to_parse, filename);
    size_t filename_len = strlen(filename);
    if(filename_len > MAX_FILENAME){
        errno = ENAMETOOLONG;
        return -1;
    }
    if(filename_len <= 0){
        errno = EINVAL;
        return -1;
    }

    int opcode = READ;

    //Invio dati al server, nell'ordine: codice op, lunghezza nome file, nome file, dimensione del file in bytes, contenuto
    if (writen(socket_fd, &opcode, sizeof(int)) <= 0) return -1;
    if (writen(socket_fd, &filename_len, sizeof(size_t)) <= 0) return -1;
    if (writen(socket_fd, filename, filename_len) <= 0) return -1;

    //Aspetto il responso, nell'ordine: dimensione del file letto e contenuto
    if (readn(socket_fd, size, sizeof(long long)) <= 0) return -1;
    if(*size == 0){
        errno = ENODATA;
        return -1;
    }
    *buf = malloc(*size); if(!buf) return -1;
    if (readn(socket_fd, *buf, *size) <= 0) return -1;

    return 0;
}

int lockFile(const char* pathname){

    if(connected == false) {
        errno = ENOTCONN;
        return -1;
    }
    if(!pathname){
        errno = EINVAL;
        return -1;
    }
    if(strlen(pathname) > MAX_PATH){
        errno = ENAMETOOLONG;
        return -1;
    }

    //Parsing nome del file
    char pathname_to_parse[MAX_ARGLEN];
    strcpy(pathname_to_parse, pathname);
    char filename[MAX_FILENAME];
    parseFilename(pathname_to_parse, filename);
    size_t filename_len = strlen(filename);
    if(filename_len > MAX_FILENAME){
        errno = ENAMETOOLONG;
        return -1;
    }
    if(filename_len <= 0){
        errno = EINVAL;
        return -1;
    }
    int opcode = LOCK;
    int errno_copy;
    int response = -1;

    do{
        //try++;
        //Invio dati al server, nell'ordine: codice op, lunghezza nome file, nome file
        if (writen(socket_fd, &opcode, sizeof(int)) <= 0) return -1;
        if (writen(socket_fd, &filename_len, sizeof(size_t)) <= 0) return -1;
        if (writen(socket_fd, filename, filename_len) <= 0) return -1;

        //Aspetto risposta
        if (readn(socket_fd, &response, sizeof(int)) <= 0) return -1;
        if (readn(socket_fd, &errno_copy, sizeof(int)) <= 0) return -1;

        //Se è andato a buon fine ok
        if (response == 0)
            return response;

    }while(errno_copy == EACCES);
    errno = errno_copy;

    if(response != 0)
        perror("lockFile");
    return response;
}

int unlockFile(const char* pathname) {
    if(connected == false) {
        errno = ENOTCONN;
        return -1;
    }
    if(!pathname){
        errno = EINVAL;
        return -1;
    }
    if(strlen(pathname) > MAX_PATH){
        errno = ENAMETOOLONG;
        return -1;
    }

    //Parsing nome del file
    char pathname_to_parse[MAX_ARGLEN];
    strcpy(pathname_to_parse, pathname);
    char filename[MAX_FILENAME];
    parseFilename(pathname_to_parse, filename);
    size_t filename_len = strlen(filename);
    if(filename_len > MAX_FILENAME){
        errno = ENAMETOOLONG;
        return -1;
    }
    if(filename_len <= 0){
        errno = EINVAL;
        return -1;
    }
    int opcode = UNLOCK;
    int errno_copy;
    int response = -1;

    //Invio dati al server, nell'ordine: codice op, lunghezza nome file, nome file
    if (writen(socket_fd, &opcode, sizeof(int)) <= 0) return -1;
    if (writen(socket_fd, &filename_len, sizeof(size_t)) <= 0) return -1;
    if (writen(socket_fd, filename, filename_len) <= 0) return -1;

    //Aspetto risposta
    if (readn(socket_fd, &response, sizeof(int)) <= 0) return -1;
    if (readn(socket_fd, &errno_copy, sizeof(int)) <= 0) return -1;
    errno = errno_copy;

    //Se è andato a buon fine ok
    if (response == 0) {
        if (strcmp(lastOpenedFlags, filename) == 0) strcpy(lastOpenedFlags, "");
        return response;
    }

    if(response != 0)
        perror("unlockFile");

    return response;
}

int removeFile(const char* pathname){

    if(connected == false) {
        errno = ENOTCONN;
        return -1;
    }
    if(!pathname){
        errno = EINVAL;
        return -1;
    }
    if(strlen(pathname) > MAX_PATH){
        errno = ENAMETOOLONG;
        return -1;
    }

    //Parsing nome del file
    char pathname_to_parse[MAX_ARGLEN];
    strcpy(pathname_to_parse, pathname);
    char filename[MAX_FILENAME];
    parseFilename(pathname_to_parse, filename);
    size_t filename_len = strlen(filename);
    if(filename_len > MAX_FILENAME){
        errno = ENAMETOOLONG;
        return -1;
    }
    if(filename_len <= 0){
        errno = EINVAL;
        return -1;
    }
    int opcode = REMOVE;
    int errno_copy;
    int response = -1;

    //Invio dati al server, nell'ordine: codice op, lunghezza nome file, nome file
    if (writen(socket_fd, &opcode, sizeof(int)) <= 0) return -1;
    if (writen(socket_fd, &filename_len, sizeof(size_t)) <= 0) return -1;
    if (writen(socket_fd, filename, filename_len) <= 0) return -1;

    //Aspetto risposta
    if (readn(socket_fd, &response, sizeof(int)) <= 0) return -1;
    if (readn(socket_fd, &errno_copy, sizeof(int)) <= 0) return -1;
    errno = errno_copy;

    //Se è andato a buon fine ok
    if (response == 0) {
        if(strcmp(lastOpenedFlags, filename) == 0) strcpy(lastOpenedFlags, "");
        return response;
    }

    if(response != 0)
        perror("Remove file");

    return response;

}





