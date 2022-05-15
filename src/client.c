#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <signal.h>
#include <API.h>
#include <time.h>
#include <sys/stat.h>

char sockname[MAX_SOCKNAME];

enum flags_   {
    O_CREATE = 1 << 0,
    O_LOCK  = 1 << 1
};

//Stampa comandi disponibili
static void print_help();

void intHandler();

int checkArgument(const char* arg);

int readFileContent(char* pathname);


int main(int argc, char* argv[]) {


    /*** VARIABILI CLIENT ******/
    bool printInfo = false; //Abilita le stampe
    long int msec = 0; //Tempo tra due richieste
    struct timespec timeLimit;
    clock_gettime(CLOCK_REALTIME, &timeLimit);
    timeLimit.tv_sec += TIMEOUT_CONN_SEC; //Tempo limite per riconnessione

    /*Inizio gestione input*/

    int opt; //identificativo opt
    bool h_opt = false; //ci dice se ho già usato il comando "-h"
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
            printInfo = true;
    }

    while (get_opt || ((opt = getopt(argc, argv, ":hf:w:W:D:r:d:R:t:l:u:c:pq")) != -1)) {

        get_opt = false;

        switch (opt) {

            case 'h':
                if (!h_opt) print_help();    //evita stampe multiple
                h_opt = true;
                break;

            case 'f':
                if (!f_opt) {//evita connessioni multiple

                    if (checkArgument(optarg) == -1) {
                        optind--;
                        get_opt = false;
                        printf("Option -f requires an argument\n");
                        break;
                    }

                    size_t len = strlen(optarg);
                    if (len > MAX_SOCKNAME) {
                        printf("Nome socket troppo lungo, massimo %d caratteri\n", MAX_SOCKNAME);
                        return -1;
                    }

                    strncpy(sockname, optarg, MAX_SOCKNAME - 1);

                    //Eventuale attesa
                    if (msec > 0)
                        sleep(msec / 1000);

                    if (openConnection(sockname, RETRY_CONN_MSEC, timeLimit) == 0) {
                        f_opt = true;
                    }
                } else
                    printf("Sei già connesso alla socket %s\n", sockname);
                break;

            case 'w':
                //TODO
            {
                char pathname[MAX_ARGLEN];
                if (checkArgument(optarg) == -1) {
                    optind--;
                    get_opt = false;
                    printf("Option -w requires an argument\n");
                    break;
                } else {
                    strcpy(pathname, optarg);
                }
                opt = getopt(argc, argv, ":hf:w:W:D:r:R:d:t:l:u:c:p");
                if (opt == -1) {

                } else {
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
                            //todo opero con la cartella
                            break;
                        case '?':
                            get_opt = true;
                            break;
                        default:
                            get_opt = true;
                    }
                    break;
                }
                //dato che "optarg" è il nome della cartella vado ricorsivamente
                //a fare per ogni file la "writeFile"
                //invia file dalla cartella
                char name[MAX_FILENAME];
                parseFilename(pathname, name);
                printf("%s", name);
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
                //TODO
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
                            //todo opero con la cartella
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
                if (!f_opt) {
                    printf("Connessione al server assente, impossibile completare la richiesta\n");
                    get_opt = false;
                    break;
                }

                //invia file specificati senza opzione -D
                char *filepath;
                char *rest = NULL;
                filepath = strtok_r(argument, ",", &rest);

                while (filepath != NULL) {
                    if (openFile(filepath, O_CREATE | O_LOCK) == 0)
                        printf("File aperto\n");
                    /*if (writeFile(filepath, directory) != 0) {
                        printf("Impossibile scrivere il file\n");
                    } else {
                        printf("FIle scritto\n");
                    }*/
                    /*if (closeFile(filepath) == 0)
                        printf("File chiuso\n");*/

                    filepath = strtok_r(NULL, ",", &rest);
                }
                printf("Termine\n");
                if (directory)
                    free(directory);

                break;
            }
            case 'D':
                printf("Nessuna operazione -w o -W valida associata al comando -D\n");
                //return -1;//TODO: ERRORE
                break;

            case 'r':
            {
                if (checkArgument(optarg) == -1) {
                    optind--;
                    get_opt = false;
                    printf("Option -r requires an argument\n");
                    break;
                }
                char pathname[MAX_ARGLEN];
                strcpy(pathname, optarg);
                //TODO
                opt = getopt(argc, argv, ":hf:w:W:D:r:R:d:t:l:u:c:p");
                if (opt == -1) {

                } else {
                    switch (opt) {
                        case 'd':
                            if (!f_opt) {
                                printf("Connessione al server assente, impossibile completare la richiesta\n");
                                get_opt = false;
                                break;
                            }
                            get_opt = false;
                            //todo opero con la cartella
                            break;
                        case '?':
                            get_opt = true;
                            break;
                        default:
                            get_opt = true;
                    }
                    break;
                }
                //Legge file specificati

                if (openFile(pathname, O_LOCK | O_CREATE) == 0){
                    printf("File aperto correttamente\n");
                }else{
                    printf("File non aperto\n");
                }
                if(writeFile(pathname, NULL) == 0){
                    printf("File letto");
                }
                if (removeFile(pathname) == 0) printf("File rimosso\n");
               /* if (openFile(pathname, 0) == 0){
                    printf("File aperto correttamente\n");
                }else{
                    printf("File non aperto\n");
                }*/
                //sleep(5);
                //if (lockFile(pathname) == 0) printf("File locked");
                fflush(stdout);
                //sleep(5);
                //if (unlockFile(pathname) == 0) printf("File unlocked");


                //void *buf;
                //size_t size;
                /*if (readFile(pathname, &buf, &size) == 0){
                    printf("File letto correttamente\n");
                    FILE *f = fopen("/home/linuxlite/Scrivania/ricevi/fileRicevuto.txt", "wb");
                    if (fwrite(buf, size, 1, f) != 1) {
                    }
                }else{
                    printf("File non letto\n");
                }*/

                break;
        }
            case 'R': {

                if (R_has_args) {
                    //caso R: con argomento obbligato
                    //Devo controllare che tipo di argomento è, se un altro comando o numero o testo ecc..
                    if (strcmp(optarg, "-d") == 0) { //TODO -------- [Caso -R -d ...]
                        optind--;
                        opt = getopt(argc, argv, ":hf:w:W:D:r:Rd:t:l:u:c:p");
                        switch (opt) {
                            case 'd':
                                if (!f_opt) {
                                    printf("Connessione al server assente, impossibile completare la richiesta\n");
                                    get_opt = false;
                                    break;
                                }
                                get_opt = false;
                                //todo opero con la cartella -d insieme leggere tutti i files
                                printf("Leggo tutti i files con opzione -d\n");
                                R_has_args = false;
                                break;
                            case ':':
                                printf("Option -d requires an argument\n");
                                return -1;
                            default:
                                return -1;
                        }
                        break;
                    }
                    //TODO ----------- caso -R -x ...
                    if (checkArgument(optarg) == -1) {
                      //  printf("%s", optarg);
                      // Se l'argomento è un altro comando diverso da -d
                        R_has_args = false;
                        optind--;
                        opt = getopt(argc, argv, ":hf:w:W:D:r:Rd:t:l:u:c:p");
                        get_opt = true;

                        //TODO Leggo tutti i files dal server senza -d
                        if (!f_opt) {
                            printf("Connessione al server assente, impossibile completare la richiesta\n");
                            get_opt = false;
                            break;
                        }
                        printf("Leggo tutti i files  senza -d\n");
                        break;
                    }

                    //TODO ---------- caso -R n
                    //Se l'argomento non è un numero do errore
                    long int n = 0;
                    if (isNumber(optarg, &n) != 0) {
                        printf("L'argomento del comando -R non è valido\n");
                        break;
                    } else {

                        //Controllo se dopo c'è -d
                        opt = getopt(argc, argv, ":hf:w:W:D:r:R:d:t:l:u:c:p");
                       // printf("%c\n", opt);
                        if (opt == -1) { //TODO ------- caso -R n ]
                            //TODO leggo N files dal server senza cartella
                            /*readNFiles(2, "/home/linuxlite/Scrivania/ricevi");*/
                            if (!f_opt) {
                                printf("Connessione al server assente, impossibile completare la richiesta\n");
                                get_opt = false;
                                break;
                            }
                            printf("Leggo N files senza -d\n");
                            break;
                        } else {

                            if(opt != 'd'){
                                //TODO leggo N files dal server senza cartella
                               // readNFiles(2, "/home/linuxlite/Scrivania/ricevi");
                                if (!f_opt) {
                                    printf("Connessione al server assente, impossibile completare la richiesta\n");
                                    get_opt = false;
                                    break;
                                }
                                printf("Leggo N files senza -d\n");
                                get_opt = true;
                                break;
                            }else{
                                if(optarg == 0){
                                    printf("Option -d requires an argument\n");
                                    return -1;
                                }
                                if (!f_opt) {
                                    printf("Connessione al server assente, impossibile completare la richiesta\n");
                                    get_opt = false;
                                    break;
                                }
                                get_opt = false;
                                //TODO leggo N files con opzione -d
                                printf("Leggo N files con opzione -d\n");
                                break;
                            }
                        }
                    }
                } else {
                    //TODO operazione Read tutti i files senza niente
                    if (!f_opt) {
                        printf("Connessione al server assente, impossibile completare la richiesta\n");
                        get_opt = false;
                        break;
                    }
                    printf("Leggo tutti i files senza opzione -d\n");
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
                }
                break;

            case 'l':
                if(checkArgument(optarg) == -1){
                    optind--;
                    get_opt = false;
                    printf("Option -l requires an argument\n");
                    break;
                }

                if(!f_opt){
                    printf("Connessione al server assente, impossibile completare la richiesta\n");
                    break;
                }
                // strtok di optarg su "," e poi
                //lock sui file specificati
                //invia file specificati senza opzione -D
                char *filepath;
                char *rest = NULL;
                filepath = strtok_r(optarg, ",", &rest);

                while (filepath != NULL) {
                    if(msec > 0)
                        sleep(msec/1000);
                    /*if (lockFile(filepath) == 0){
                        if(printInfo) printf("OPERAZIONE:  Lock file \nFILE: %s \nESITO: Priorità acquisita correttamente\n", filepath);
                    }else{
                        if(printInfo) printf("OPERAZIONE: Lock file \nFILE: %s \nESITO: Impossibile acquisire la priorità sul file\n", filepath);
                    }*/
                    filepath = strtok_r(NULL, ",", &rest);
                }
                break;

            case 'u':
                if(checkArgument(optarg) == -1){
                    optind--;
                    get_opt = false;
                    printf("Option -u requires an argument\n");
                    break;
                }
                if(!f_opt){
                    printf("Connessione al server assente, impossibile completare la richiesta\n");
                    break;
                }

                filepath = NULL;
                rest = NULL;
                filepath = strtok_r(optarg, ",", &rest);

                while (filepath != NULL) {
                    if(msec > 0)
                        sleep(msec/1000);
                   /* if (unlockFile(filepath) == 0){
                        if(printInfo){
                            printf("---------------------------------\n"
                                   "OPERAZIONE:  Unlock file \nFILE: %s \nESITO: Priorità rilasciata correttamente\n"
                                   "---------------------------------\n",
                                   filepath);
                        }
                    }else{
                        if(printInfo) printf("=====================================\n"
                                             "OPERAZIONE: Unlock file \nFILE: %s \nESITO: Impossibile rilasciare la priorità sul file\n"
                                             "=====================================\n",
                                             filepath);
                    }*/
                    filepath = strtok_r(NULL, ",", &rest);
                }
                break;

            case 'c':
                if(checkArgument(optarg) == -1){
                    optind--;
                    get_opt = false;
                    printf("Option -c requires an argument\n");
                    break;
                }
                if(!f_opt){
                    printf("Connessione al server assente, impossibile completare la richiesta\n");
                    break;
                }
                //cancella i file specificati dal server
                // strtok di optarg su "," e poi remove di ognuno
                filepath = NULL;
               rest = NULL;
                filepath = strtok_r(optarg, ",", &rest);

                while(filepath != NULL) {
                    if(openFile(filepath, 0) == 0)
                        printf("File aperto\n");
                   /* if (removeFile(filepath) != 0){
                        printf("Impossibile eliminare il file\n");
                    }else{
                        printf("File eliminato\n");
                    }*/
                    /*if (closeFile(filepath) == 0)
                        printf("File chiuso\n");*/

                    filepath = strtok_r(NULL, ",", &rest);
                }
                break;

            case 'p':
                //abilita le stampe per le operazioni
                if (!printInfo) {
                    printInfo = true;
                }
                break;

            case 'q':
             //   quit();
                break;

            case '?':
                printf("Comando sconosciuto -%c\n", optopt);
                break;

            case ':':
            {
                   if(optopt == 'R') {
                       R_has_args = false;
                       optind--;
                       opt = getopt(argc, argv, ":hf:w:W:D:r:Rd:t:l:u:c:p");
                       get_opt = true;
                       break;
                   }else{
                       printf("Option %c requires an argument\n", optopt);
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
}

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

int readFileContent(char* pathname){

    FILE* file;
    file = fopen(pathname, "rb+");
    if(!file){
        perror("Open file");
        return -1;
    }
    struct stat sb;
    if (stat(pathname, &sb) == -1){
        printf("ERRORE");
    }

    size_t file_size = sb.st_size;
    void* data = malloc(file_size);
    while (fread(data, 1, file_size, file) > 0){

    }
    //  ((char*)(data))[file_size] = '\0';
    printf("%s", (char*)data);
    fclose(file);
    return 0;
}
