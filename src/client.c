#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <signal.h>
#include <API.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>

char sockname[MAX_SOCKNAME];

enum flags_   {
    O_CREATE = 1 << 0,
    O_LOCK  = 1 << 1
};

int N_global = 0;
unsigned int sec = 0;

//Stampa comandi disponibili
static void print_help();

int checkArgument(const char* arg);

int isdot(const char dir[]);

int readFileContent(char* pathname, void** buf, size_t* size);

//Analizza ricorsivamente la cartella nomedir ed esegue le operazioni relative a -w (writeFile)
int lsR(char* nomedir, char* store_dirname);

int main(int argc, char* argv[]) {

    /*** VARIABILI CLIENT ******/
    long int msec = 0; //Tempo tra due richieste
    extern unsigned int sec;
    bool time_set = false;
    extern bool print_info;
    struct timespec timeLimit;
    if( clock_gettime(CLOCK_REALTIME, &timeLimit) == -1)
        exit(EXIT_FAILURE);
    timeLimit.tv_sec += TIMEOUT_CONN_SEC; //Tempo limite per riconnessione
    extern int N_global;

    /*Inizio gestione input*/

    int opt; //identificativo opt
    //bool h_opt = false; //ci dice se ho già usato il comando "-h"
    bool f_opt = false; //ci dice se ho già usato con successo il comando "-f"
    bool get_opt = false;
    bool R_has_args = true;
    opterr = 0;

    //Controlla lunghezza argomenti e controlla se è presente "-p" per attivare le stampe
    for(int i = 0; i < argc; i++){
        if(strlen(argv[i]) > MAX_ARGLEN) {
            printf("\nArgomento %d troppo lungo, terminato\n", i);
            exit(EXIT_FAILURE);
        }
        if(strcmp(argv[i], "-p") == 0)
            print_info = true;
        //Se trova -h stampa e termina
        if(strcmp(argv[i], "-h") == 0) {
            print_help();
            exit(EXIT_SUCCESS);
        }
    }

    while (get_opt || ((opt = getopt(argc, argv, ":hf:w:W:D:r:d:R:t:l:u:c:pq")) != -1)) {
        get_opt = false;

        switch (opt) {
            case 'f':
                if (!f_opt) {   //evita connessioni multiple
                    if (checkArgument(optarg) == -1) {
                        optind--;
                        get_opt = false;
                        printf("\nOption f requires an argument\n");
                        goto cleanup;
                    }
                    size_t len = strlen(optarg);
                    if (len > MAX_SOCKNAME) {
                        printf("\nNome socket troppo lungo, massimo %d caratteri\n", MAX_SOCKNAME);
                        goto cleanup;
                    }
                    strncpy(sockname, optarg, MAX_SOCKNAME - 1);
                    if (openConnection(sockname, RETRY_CONN_MSEC, timeLimit) == 0)
                        f_opt = true;
                    if(sec > 0) sleep(sec);
                }
                break;


            case 'w': //-w dirname[,n=0]
            {   char dirname[MAX_PATH];
                char* store_dirname = NULL;
                long N = 0;
                //Se non ha argomento
                if (checkArgument(optarg) == -1) {
                    optind--;
                    get_opt = false;
                    printf("\nOption -w requires an argument\n");
                    goto cleanup;
                    break;
                } else {
                    //Divido il nome della cartella da N se presente
                    char *token;
                    char *rest = NULL;
                    token = strtok_r(optarg, ",", &rest);
                    strncpy(dirname, token, MAX_PATH);
                    token = strtok_r(NULL, ",", &rest);
                    if(token)
                     if(isNumber(token, &N) != 0) {
                         printf("\nOption -w: il secondo parametro non è un numero valido\n");
                         goto cleanup;
                     }
                }
                opt = getopt(argc, argv, ":hf:w:W:D:r:R:d:t:l:u:c:p");
                if (opt == -1) {
                //Non ho associato l'opzione -D
                } else { //Ho associato l'opzione -D, ne controllo la validità
                    switch (opt) {
                        case 'D':
                            if (checkArgument(optarg) == -1) {
                                optind--;
                                get_opt = false;
                                printf("\nOption -D requires an argument\n");
                                goto cleanup;
                                break;
                            }
                            get_opt = false;
                            //Salvo il nome della cartella
                            store_dirname = (char*)malloc(sizeof(char)*MAX_PATH);
                            strncpy(store_dirname, optarg, MAX_ARGLEN-1);
                            break;
                        case '?':
                            get_opt = true;
                            break;
                        default:
                            get_opt = true;
                    }
                }
                //Svolgo l'operazione: vado ricorsivamente ad analizzare la cartella
                //finché non leggo tutti i file o finché non leggo N files
                if(N <= 0) N = -1;
                N_global = (int)N;
                lsR(dirname, store_dirname);
                if(store_dirname) free(store_dirname);
                break;
            }



            case 'W': {
                char argument[MAX_ARGLEN];
                char *directory = NULL;
                if (checkArgument(optarg) == -1) {
                    optind--;
                    get_opt = false;
                    printf("\nOption -W requires an argument\n");
                    goto cleanup;
                    break;
                } else {
                    //Mi salvo optarg
                    strcpy(argument, optarg);
                }
                opt = getopt(argc, argv, ":hf:w:W:D:r:R:d:t:l:u:c:p");
                if (opt == -1) {
                } else {
                    switch (opt) {
                        case 'D':
                            if (checkArgument(optarg) == -1) {
                                optind--;
                                get_opt = false;
                                printf("\nOption -D requires an argument\n");
                                goto cleanup;
                                break;
                            }
                            get_opt = false;
                            //Devo salvare la cartella
                            directory = malloc(sizeof(char) * strlen(optarg) + 1);
                            if (!directory) {
                                perror("\nFallimento allocazione memoria");
                                exit(EXIT_FAILURE);
                            }
                            strcpy(directory, optarg);
                            break;
                        case '?':
                            get_opt = true;
                            break;
                        default:
                            get_opt = true;
                    }
                }
                char *filepath;
                char *rest = NULL;
                filepath = strtok_r(argument, ",", &rest);
                //Per ogni file
                while (filepath != NULL) {
                    if ( openFile(filepath, O_LOCK | O_CREATE) != 0){
                        //Se fallisco la openFile con i flags provo ad aprirlo senza creazione
                        // per fare la append
                        if (openFile(filepath, 0) == 0){
                            void* buf;
                            size_t size;
                            if (readFileContent(filepath, &buf, &size) == 0){
                                appendToFile(filepath, buf, size, directory);
                            }
                            closeFile(filepath);
                        }else{
                            printf("\nErrore nell'apertura del file %s\n", filepath);
                            goto cleanup;
                        }
                    }else{
                        //Se invece ho potuto crearlo e bloccarlo ne faccio la scrittura
                        writeFile(filepath, directory);
                        closeFile(filepath);
                    }
                    filepath = strtok_r(NULL, ",", &rest);
                    if(sec > 0) sleep(sec);
                }
                if (directory)
                    free(directory);
                break;
            }
            case 'D':
                printf("\nNessuna operazione -w o -W valida associata al comando -D\n");
                break;




            case 'r':
            {
                if (checkArgument(optarg) == -1) {
                    optind--;
                    get_opt = false;
                    printf("\nOption -r requires an argument\n");
                    goto cleanup;
                    break;
                }
                char pathname[MAX_ARGLEN] = "";
                char dirname[MAX_PATH] = "";
                bool set_dir = false;
                strcpy(pathname, optarg);
                opt = getopt(argc, argv, ":hf:w:W:D:r:R:d:t:l:u:c:p");
                if (opt == -1) {

                } else {
                    switch (opt) {
                        case 'd':
                            get_opt = false;
                            strcpy(dirname, optarg);
                            DIR* dir = opendir(dirname);
                            if(!dir) {
                                printf("\nErrore nell'apertura della cartella %s\n", dirname);
                                goto cleanup;
                            }
                            set_dir = true;
                            break;
                        case '?':
                            get_opt = true;
                            break;
                        default:
                            get_opt = true;
                            break;
                    }
                }
                char* filepath = NULL;
                char* rest = NULL;
                filepath = strtok_r(pathname, ",", &rest);
                //Per ogni file
                while (filepath != NULL) {
                    char filepath_copy[MAX_FILENAME];
                    strcpy(filepath_copy, filepath);
                    openFile(filepath, 0);
                    //Legge file specificati
                    void *buf = NULL;
                    size_t size;
                    if (readFile(filepath, &buf, &size) == 0 && set_dir) {
                        char filename_parsed[MAX_FILENAME];
                        parseFilename(filepath, filename_parsed);
                        char complete_pathname[MAX_PATH*2];
                        if(dirname[strlen(dirname)] != '/') strcat(dirname, "/");
                        strcat(complete_pathname, dirname);
                        strcat(complete_pathname, filename_parsed);

                        FILE *f = fopen(complete_pathname, "wb");
                        if (!f) {
                            PRINT_INFO(print_info,
                                       "\nreadFile: apertura del file %s nel dispositivo locale. Info sull'errore %s",
                                       complete_pathname,
                                       strerror(errno))
                           goto cleanup;
                        }
                        if (fwrite(buf, size, 1, f) != 1) {
                            PRINT_INFO(print_info, "\nreadFile: scrittura del file %s nel dispositivo locale. Info sull'errore %s", pathname,
                                       strerror(errno))
                           goto cleanup;
                        }
                        PRINT_INFO(print_info, "\nClient: memorizzazione del file %s appena letto nella cartella %s", filename_parsed, dirname);
                        if(buf){
                            free(buf);
                            buf = NULL;
                        }
                    }
                    if(buf){
                        free(buf);
                        buf = NULL;
                    }
                    closeFile(filepath_copy);
                    filepath = strtok_r(NULL, ",", &rest);
                    if(sec > 0) sleep(sec);
                }
                break;
        }


        /* Commento: il parsing di questo comando è stato gestito
         * in modo più complicato per essere in grado di adattarsi ai diversi casi
         * (ad es. comando scritto in fondo, comando senza argomenti ma con -d, comando con N e -d ecc..) */
            case 'R': {
                int N = 0;
                char dirname[MAX_PATH];

                if (R_has_args) {
                    //caso R: con argomento obbligato
                    //Devo controllare che tipo di argomento è, se un altro comando o numero o testo ecc..

                    // Caso -R -d [...]
                    if (strcmp(optarg, "-d") == 0) {
                        optind--;
                        opt = getopt(argc, argv, ":hf:w:W:D:r:Rd:t:l:u:c:p");
                        switch (opt) {
                            case 'd':
                                get_opt = false;
                                //Salvo il nome della cartella
                                strcpy(dirname, optarg);
                                readNFiles(0, dirname);
                                R_has_args = false;
                                if(sec > 0) sleep(sec);
                                break;
                            case ':':
                                printf("\nOption -d requires an argument\n");
                                goto cleanup;
                            default:
                                break;
                        }
                        break;
                    }


                    //caso -R -x ...
                    if (checkArgument(optarg) == -1) {
                      // Se l'argomento è un altro comando diverso da -d
                        R_has_args = false;
                        optind--;
                        opt = getopt(argc, argv, ":hf:w:W:D:r:Rd:t:l:u:c:p");
                        get_opt = true;

                        //Leggo tutti i files dal server senza -d
                        readNFiles(0, NULL);
                        if(sec > 0) sleep(sec);
                        break;
                    }

                    //Caso -R n
                    //Se l'argomento non è un numero do errore
                    long int n = 0;
                    if (isNumber(optarg, &n) != 0) {
                        printf("\nL'argomento del comando -R non è valido\n");
                        goto cleanup;
                    } else { //L'argomento è un numero valido, caso -R n
                        N = (int)n;
                        //Controllo cosa c'è dopo -R n
                        opt = getopt(argc, argv, ":hf:w:W:D:r:R:d:t:l:u:c:p");
                        if (opt == -1) { //caso -R n ] (end of line)
                            //leggo N files dal server senza cartella
                            readNFiles(N, NULL);
                            if(sec > 0) sleep(sec);
                            break;
                        } else { //Dopo -R n c'è qualcos'altro

                            if(opt == ':' && optopt == 'd'){
                                //c'è -d senza argomento
                                printf("\nOption -d requires an argument\n");
                                goto cleanup;
                            }

                            if(opt == 'd'){
                                //c'è -d con argomento
                                get_opt = false;
                                //leggo N files con opzione -d
                                strcpy(dirname, optarg);
                                readNFiles(N, dirname);
                                if(sec > 0) sleep(sec);
                                break;
                            }
                            //comando diverso da -d
                            get_opt = true;
                            readNFiles(N, NULL);
                            if(sec > 0) sleep(sec);
                            break;
                        }
                    }
                } else { //-R
                    //operazione Read tutti i files senza niente
                    readNFiles(0, NULL);
                    if(sec > 0) sleep(sec);
                    break;
                }
            }

            case 'd':
                printf("\nNessuna operazione -r o -R valida associata al comando -d\n");
                goto cleanup;

            case 't':
                if(checkArgument(optarg) == -1){
                    optind--;
                    get_opt = false;
                    printf("\nOption -t requires an argument\n");
                    goto cleanup;
                }
                if (isNumber(optarg, &msec) != 0) {
                    printf("\nErrore nel formato del comando -t, valore non impostato\n");
                    msec = 0;
                }else{
                    sec = msec/1000;
                    time_set = true;
                }
                break;


            case 'l':
                if(checkArgument(optarg) == -1){
                    optind--;
                    get_opt = false;
                    printf("\nOption -l requires an argument\n");
                    goto cleanup;
                    break;
                }
                char *filepath;
                char *rest = NULL;
                filepath = strtok_r(optarg, ",", &rest);
                //Per ogni file
                while (filepath != NULL) {
                    openFile(filepath, 0);
                    lockFile(filepath);
                    closeFile(filepath);
                    filepath = strtok_r(NULL, ",", &rest);
                    if(sec > 0) sleep(sec);
                }
                break;

            case 'u':
                if(checkArgument(optarg) == -1){
                    optind--;
                    get_opt = false;
                    printf("\nOption -u requires an argument\n");
                    goto cleanup;
                    break;
                }
                filepath = NULL;
                rest = NULL;
                filepath = strtok_r(optarg, ",", &rest);
                //Per ogni file
                while (filepath != NULL) {
                    openFile(filepath, 0);
                    unlockFile(filepath);
                    closeFile(filepath);
                    filepath = strtok_r(NULL, ",", &rest);
                    if(sec > 0) sleep(sec);
                }
                break;


            case 'c':
                if(checkArgument(optarg) == -1){
                    optind--;
                    get_opt = false;
                    printf("\nOption -c requires an argument\n");
                    goto cleanup;
                    break;
                }
                filepath = NULL;
                rest = NULL;
                filepath = strtok_r(optarg, ",", &rest);
                //per ogni file
                while(filepath != NULL) {
                    openFile(filepath, 0);
                    lockFile(filepath);
                    removeFile(filepath);
                    filepath = strtok_r(NULL, ",", &rest);
                }
                break;


            case 'p':
                break;


            case '?':
                printf("\nComando sconosciuto -%c\n", optopt);
                break;


            case ':':
            {         if(optopt == 'R') {
                       R_has_args = false;
                       optind--;
                       opt = getopt(argc, argv, ":hf:w:W:D:r:Rd:t:l:u:c:p");
                       get_opt = true;
                       break;
                   }else{
                       printf("\nOption %c requires an argument\n", optopt);
                       goto cleanup;
                       break;
                   }
            }

            default:
                printf("\nComando sconosciuto -%c\n", optopt);
                break;
        }
    }

    cleanup:
    closeConnection(sockname);
    printf("\nTerminazione..\n");
    return 0;
}





/***************** FUNZIONI AGGIUNTIVE ******************/

static void print_help(){

    printf("Comandi disponibili:\n\
    -f filename: specifica il nome del socket AF_UNIX a cui connettersi \n\
    -w dirname[,n=0]: invia al server al massimo 'n' file presenti nella cartella ‘dirname’, se n=0 vengono inviati tutti, se possibile \n\
    -W file1[,file2]: lista di nomi di file da scrivere nel server separati da ‘,’ \n\
    -D dirname: cartella dove ricevere i file che il server rimuove a seguito di capacity misses, da usare dopo -w o -W \n\
    -r file1[,file2]: lista di nomi di file da leggere dal server separati da ‘,’\n\
    -R [n=0]: legge ‘n’ file qualsiasi attualmente memorizzati nel server, se n=0 o non specificato allora vengono letti tutti i file presenti nel server\n\
    -d dirname: cartella dove scrivere i file letti dal server da usare dopo ‘-r’ o ‘-R’\n\
    -t time: tempo in millisecondi che intercorre tra l’invio di due richieste successive \n\
    -l file1[,file2]: lista di nomi di file su cui acquisire la mutua esclusione\n\
    -u file1[,file2]: lista di nomi di file su cui rilasciare la mutua esclusione\n\
    -c file1[,file2]: lista di file da rimuovere dal server se presenti \n\
    -p: abilita le stampe sullo standard output per ogni operazione\n   ");

}


void intHandler() {
    fflush(stdout);
    if ( closeConnection(sockname) != 0){
        printf("Cannot close connection\n");
        perror("Close connection");
    }
    printf("\n Disconnesso, bye!\n");
    fflush(stdout);
    exit(1);
}//TODO

int checkArgument(const char* arg) {

    if ( !optarg || arg[0] == '-'
         || (strcmp(optarg, "-h") == 0) || (strcmp(optarg, "-f") == 0)
         || (strcmp(optarg, "-w") == 0) || (strcmp(optarg, "-W") == 0)
         || (strcmp(optarg, "-D") == 0) || (strcmp(optarg, "-r") == 0)
         || (strcmp(optarg, "-R") == 0) || (strcmp(optarg, "-d") == 0)
         || (strcmp(optarg, "-t") == 0) || (strcmp(optarg, "-l") == 0)
         || (strcmp(optarg, "-u") == 0) || (strcmp(optarg, "-c") == 0)
         || (strcmp(optarg, "-p") == 0)
            ) {
        return -1;
    }else
        return 0;
}


int isdot(const char dir[]) {
    int l = (int)strlen(dir);

    if ( (l>0 && dir[l-1] == '.') ) return 1;
    return 0;
}

int lsR(char* nomedir, char* store_dirname) {
    extern int N_global;
    if(N_global == 0)
        return 0;
    if(!nomedir)
        return -1;

    // controllo che il parametro sia una directory
    struct stat statbuf_;
    if (stat(nomedir,&statbuf_) != 0){
        perror("Eseguendo la stat");
        exit(EXIT_FAILURE);
    }
    DIR * dir;

    if ((dir=opendir(nomedir)) == NULL) {
        perror("opendir");
        fprintf(stderr, "Errore aprendo la directory %s\n", nomedir);
        return -1;
    } else {
        struct dirent *file;

        while((errno=0, file =readdir(dir)) != NULL) {
            struct stat statbuf;
            char filename[MAX_PATH];
            int len1 = (int)strlen(nomedir);
            int len2 = (int)strlen(file->d_name);
            if ((len1+len2+2)>MAX_PATH) {
                fprintf(stderr, "ERRORE: MAX_PATH troppo piccolo\n");
                exit(EXIT_FAILURE);
            }
            strncpy(filename,nomedir,      MAX_PATH-1);
            if(nomedir[strlen(nomedir)-1] != '/') strncat(filename,"/",          MAX_PATH-1);
            strncat(filename,file->d_name, MAX_PATH-1);

            if (stat(filename, &statbuf)==-1) {
                perror("eseguendo la stat");
                fprintf(stderr, "Errore nel file %s\n", filename);
                return -1;
            }
            if(S_ISDIR(statbuf.st_mode)) {
                if ( !isdot(filename) ) lsR(filename, store_dirname);
            } else {
                if ( openFile(filename, O_LOCK | O_CREATE) != 0){
                    //prova ad aprirlo almeno senza creazione
                    if (openFile(filename, 0) == 0){
                        void* buf;
                        size_t size;
                        if (readFileContent(filename, &buf, &size) == 0){
                            appendToFile(filename, buf, size, store_dirname);
                            free(buf);
                        }
                        closeFile(filename);
                    }
                }else{
                    writeFile(filename, store_dirname);
                    closeFile(filename);
                }
                N_global--;
                if(sec > 0) sleep(sec);
            }
        }
        if (errno != 0) perror("readdir");
        closedir(dir);
    }
    return 0;
}


int readFileContent(char* pathname, void** buf, size_t* size){
    if(!pathname || !buf || !size) {
        fprintf(stderr, "\nClient: errore lettura contenuto file %s. Info sull'errore: %s\n", pathname,
                strerror(errno));
        errno = EINVAL;
        return -1;
    }
    FILE* file;
    file = fopen(pathname, "rb+");
    if(!file){
        fprintf(stderr, "\nClient: errore lettura contenuto file %s. Info sull'errore: %s\n", pathname,
                strerror(errno));
        return -1;
    }
    struct stat sb;
    if (stat(pathname, &sb) == -1){
        fprintf(stderr, "\nClient: errore lettura statistiche file %s. Info sull'errore: %s\n", pathname,
                strerror(errno));
        return -1;
    }
    *size = sb.st_size;
    if(*size <= 0){
        errno = ENODATA;
        fprintf(stderr, "\nClient: errore lettura contenuto file %s. Info sull'errore: %s\n", pathname,
                strerror(errno));
        return -1;
    }
    *buf = malloc(*size);
    if(!*buf){
        perror("Errore allocazione memoria");
        exit(EXIT_FAILURE);
    }
    while (fread(*buf, 1, *size, file) > 0){

    }
    fclose(file);
    return 0;
}