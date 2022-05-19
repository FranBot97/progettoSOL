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
    struct timespec timeLimit;
    clock_gettime(CLOCK_REALTIME, &timeLimit);
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
            printf("Argomento troppo lungo, riprovare\n");
            return -1;
        }
        if(strcmp(argv[i], "-p") == 0)
            print_info = true;
        //Se trova -h stampa e termina
        if(strcmp(argv[i], "-h") == 0) {
            print_help();
            return 0;
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
                        printf("Option f requires an argument\n");
                        break;
                    }
                    size_t len = strlen(optarg);
                    if (len > MAX_SOCKNAME) {
                        printf("Nome socket troppo lungo, massimo %d caratteri\n", MAX_SOCKNAME);
                        return -1;
                    }
                    strncpy(sockname, optarg, MAX_SOCKNAME - 1);
                    //Eventuale attesa
                    if (openConnection(sockname, RETRY_CONN_MSEC, timeLimit) == 0)
                        f_opt = true;
                    if(sec > 0) sleep(sec);
                }
                break;



            case 'w': //-w dirname[,n=0]
            {   char dirname[MAX_PATH];
                char store_dirname[MAX_PATH] = "";
                long N = 0;
                //Se non ha argomento
                if (checkArgument(optarg) == -1) {
                    optind--;
                    get_opt = false;
                    printf("Option -w requires an argument\n");
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
                         printf("Option -w: il secondo parametro non è un numero valido\n");
                         return -1;
                     }
                }
                opt = getopt(argc, argv, ":hf:w:W:D:r:R:d:t:l:u:c:p");
                if (opt == -1) {
                //Non ho associato l'opzione -D
                } else { //Ho associato l'opzione -D, ne controllo la validità
                    switch (opt) {
                        case 'D':
                            if (!f_opt) {
                                printf("Connessione al server assente, impossibile completare la richiesta\n");
                                get_opt = false;
                                break;
                            }
                            if (checkArgument(optarg) == -1) {
                                optind--;
                                get_opt = false;
                                printf("Option -D requires an argument\n");
                                break;
                            }
                            get_opt = false;
                            //Salvo il nome della cartella
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

                break;
            }



            case 'W': {
                char argument[MAX_ARGLEN];
                char *directory = NULL;
                if (checkArgument(optarg) == -1) {
                    optind--;
                    get_opt = false;
                    printf("Option -W requires an argument\n");
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
                                printf("Option -D requires an argument\n");
                                break;
                            }
                            get_opt = false;
                            //Devo salvare la cartella
                            directory = malloc(sizeof(char) * strlen(optarg) + 1);
                            if (!directory) {
                                perror("Malloc");
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
                printf("Nessuna operazione -w o -W valida associata al comando -D\n");
                break;




            case 'r':
            {
                if (checkArgument(optarg) == -1) {
                    optind--;
                    get_opt = false;
                    printf("Option -r requires an argument\n");
                    break;
                }
                char pathname[MAX_ARGLEN] = "";
                char dirname[MAX_PATH] = "";
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
                                perror("Operazione -r, apertura cartella\n");
                                return -1;
                            }
                            break;
                        case '?':
                            get_opt = true;
                            break;
                        default:
                            get_opt = true;
                    }
                    break;
                }
                char* filepath = NULL;
                char* rest = NULL;
                filepath = strtok_r(pathname, ",", &rest);
                //Per ogni file
                while (filepath != NULL) {
                    openFile(filepath, 0);
                    //Legge file specificati
                    void *buf = NULL;
                    size_t size;
                    if (readFile(pathname, &buf, &size) == 0) {
                        FILE *f = fopen(pathname, "wb");
                        if (!f)
                            perror("readFile: salvataggio file nel dispositivo locale");
                        if (fwrite(buf, size, 1, f) != 1)
                            perror("readFile: salvataggio file nel dispositivo locale");
                        if(buf){
                            free(buf);
                            buf = NULL;
                        }
                    }
                    if(buf){
                        free(buf);
                        buf = NULL;
                    }
                    closeFile(filepath);
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
                                printf("Option -d requires an argument\n");
                                return -1;
                            default:
                                return -1;
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
                        printf("L'argomento del comando -R non è valido\n");
                        return -1;
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
                                printf("Option -d requires an argument\n");
                                return -1;
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
                printf("Nessuna operazione -r o -R valida associata al comando -d\n");
                break;

            case 't':
                if(checkArgument(optarg) == -1){
                    optind--;
                    get_opt = false;
                    printf("Option -t requires an argument\n");
                    break;
                }
                if (isNumber(optarg, &msec) != 0) {
                    printf("Errore nel formato del comando -t, valore non impostato\n");
                    msec = 0;
                }else{
                    sec = msec/1000;
                }
                break;


            case 'l':
                if(checkArgument(optarg) == -1){
                    optind--;
                    get_opt = false;
                    printf("Option -l requires an argument\n");
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
                    printf("Option -u requires an argument\n");
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
                    printf("Option -c requires an argument\n");
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
                //abilita le stampe per le operazioni
                if (!print_info) {
                    print_info = true;
                }
                break;


            case '?':
                printf("Comando sconosciuto -%c\n", optopt);
                break;


            case ':':
            {         if(optopt == 'R') {
                       R_has_args = false;
                       optind--;
                       opt = getopt(argc, argv, ":hf:w:W:D:r:Rd:t:l:u:c:p");
                       get_opt = true;
                       break;
                   }else{
                       printf("Option %c requires an argument\n", optopt);
                       break;
                   }
            }

            default:
                printf("Comando sconosciuto -%c\n", optopt);
                break;
        }
        /***************/
    }
    closeConnection(sockname);
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
            strncat(filename,"/",          MAX_PATH-1);
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
        errno = EINVAL;
        return -1;
    }
    FILE* file;
    file = fopen(pathname, "rb+");
    if(!file){
        perror("Open file");
        return -1;
    }
    struct stat sb;
    if (stat(pathname, &sb) == -1){
        printf("ERRORE");
        return -1;
    }
    *size = sb.st_size;
    if(*size <= 0){
        errno = ENODATA;
        return -1;
    }
    *buf = malloc(*size);
    if(!*buf){
        return -1;
    }
    while (fread(*buf, 1, *size, file) > 0){

    }
    fclose(file);
    return 0;
}