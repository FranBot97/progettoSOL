#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/select.h>
#include <stdbool.h>
#include <signal.h>
#include <limits.h>
//#include <sys/stat.h>
//#include <errno.h>

#include <threadpool.h>
#include <request.h>
#include <request_handler.h>
#include <util.h>

//Funzione che calcola max fd
int updatemax(fd_set set, int fdmax);

//Funzione che legge un'intera linea del file di configurazione
int read_line(char *line, char* NOME_SOCKET, int* NUMERO_WORKERS, int* LIMITE_FILE, int* LIMITE_MB, int* METODO_RIMPIAZZAMENTO);

//Funzione che legge tutto il file di configurazione e inizializza i parametri
int read_config_file(char* config_filename, char* NOME_SOCKET, int* NUMERO_WORKERS, int* LIMITE_FILE, int* LIMITE_MB, int* METODO_RIMPIAZZAMENTO);

//Gestore segnali
bool NO_MORE_CONNECTIONS = false;
bool NO_MORE_REQUESTS = false;
static void signals_checker(void* arg);
static int signal_pipe[2]; //pipe per segnali

/****************************/
//      MAIN START
/***************************/
int main(int argc, char* argv[]) {

    printf("Server avviato\n");
    if(argc < 3){
        printf("Esegui come: server -f <nome_file.txt>\n");
        goto exit;
    }
    printf("================ INIZIALIZZAZIONE SERVER IN CORSO ==================\n");

    //DICHIARAZIONE VARIABILI
    int socket_fd = -1;
    char NOME_SOCKET[MAX_PATH] = "";
    storage_t* myStorage = NULL;
    threadpool_t* workers = NULL;
    request_t *req = NULL;
    int NUMERO_WORKERS = 0;
    int MAX_FILE = 0;
    int MAX_MEMORIA = 0;
    int REPLACE_MODE = 0; //0 = FIFO, 1 = LRU
    int pfd[2]; //pipe

    //GESTIONE SEGNALI
    struct sigaction sig_action;
    sigset_t sigset;
    memset(&sig_action, 0, sizeof(sig_action));
    sig_action.sa_handler = SIG_IGN;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGHUP);
    sigaddset(&sigset, SIGQUIT);
    sig_action.sa_mask = sigset;
    pthread_t signal_thread = 0;
    bool signal_thread_activated = false;
    //PIPE: [0] per lettura, [1] scrittura
    if(pipe(signal_pipe) == -1){
        printf("Errore nella creazione della pipe\n");
        goto cleanup;
    }
    if (pthread_sigmask(SIG_BLOCK, &sigset, NULL) != 0) goto cleanup;

    //LETTURA FILE CONFIGURAZIONE
     int opt;
     while( ( opt = getopt(argc, argv, ":f:") ) != -1 ){
         switch (opt) {
             case 'f':
                if ( read_config_file(optarg,
                                      NOME_SOCKET,
                                      &NUMERO_WORKERS,
                                      &MAX_FILE,
                                      &MAX_MEMORIA,
                                      &REPLACE_MODE) == -1)
                    goto cleanup;
                 break;
             case ':':
                 printf("Il comando -f richiede un argomento\n");
                 goto cleanup;
             case '?':
             default:
                 printf("Comando non riconosciuto\n");
                 goto cleanup;
         }
     }

     //INIZIALIZZAZIONE DOPO LETTURA FILE

     //PIPE: pfd[0] per lettura, pfd[1] scrittura
     if(pipe(pfd) == -1){
         printf("Errore nella creazione della pipe\n");
         goto cleanup;
     }
     //STORAGE:
     myStorage = storage_create(MAX_FILE, MAX_MEMORIA * (1024 * 1024), REPLACE_MODE);
     if(myStorage == NULL) {
         printf("Errore nella creazione del file storage\n");
         goto cleanup;
     }else{
         printf("- File storage creato correttamente\n");
     }
    //THREADPOOL:
     workers = createThreadPool(NUMERO_WORKERS, 10);
     if(workers == NULL){
         printf("Errore nella creazione del threadpool\n");
         goto cleanup;
     }else{
         printf("- Threadpool creato correttamente\n");
     }
     //CREAZIONE SOCKET:
     unlink(NOME_SOCKET);
     if ( (socket_fd = socket(AF_UNIX, SOCK_STREAM,0) ) == -1){
        perror("Creazione socket");
        goto cleanup;}
     struct sockaddr_un serv_addr;
     memset(&serv_addr, 0, sizeof(serv_addr));
     serv_addr.sun_family = AF_UNIX;
     strncpy(serv_addr.sun_path, NOME_SOCKET, SOCKET_LEN);
     if ( bind(socket_fd, (struct sockaddr*)&serv_addr,sizeof(serv_addr)) == -1){
       perror("Bind fallita");
       goto cleanup;}
     if(  listen(socket_fd, MAX_CONN)  == -1 ){
       perror("Listen fallita");
       goto cleanup;}
     printf("- Socket creata correttamente\n");

     printf("----- PARAMETRI ------ \n");
     printf("Numero massimo file: %d\n", MAX_FILE);
     printf("Capacità memoria file storage: %d MB\n", MAX_MEMORIA);
     printf("Numero di thread workers: %d\n", NUMERO_WORKERS);
     printf("Modalità rimpiazzamento: %s\n", (REPLACE_MODE == 0 ? "FIFO" : "NULL"));
     printf("=============== FINE INIZIALIZZAZIONE SERVER ==================\n");

     //PARAMETRI SELECT
     int connected_clients = 0;
     fd_set set, rdset;
     FD_ZERO(&set);
     FD_ZERO(&rdset);
     FD_SET(socket_fd, &set);
     FD_SET(pfd[0], &set);
     FD_SET(signal_pipe[0], &set);
     //Adesso che ho assegnato la pipe faccio partire il thread dei segnali
    if (pthread_create(&signal_thread, NULL, (void *(*)(void *)) &signals_checker, (void*)&sigset) != 0) goto cleanup;
    signal_thread_activated = true;

     int fdmax = (socket_fd > pfd[0] ? socket_fd : pfd[0]);

     printf(" In attesa di nuove connessioni...  \n");
     while(true){
       rdset = set;

       if(select(fdmax+1, &rdset, NULL, NULL, NULL) == -1){
           perror("select");
           goto cleanup;
       }
       //esamino i fd
       for(int i=0; i <= fdmax; i++){

           //Se non accetto più nessuna richiesta esco
            if(NO_MORE_REQUESTS == true){
                printf("Ok");
                goto cleanup;
            }
            //Se non accetto più nuove connessioni e tutti i client attivi si sono disconnessi esco
            if(NO_MORE_CONNECTIONS == true && connected_clients == 0){
                goto cleanup;
            }

           //Se c'è una richiesta di lettura
           if(FD_ISSET(i, &rdset)) {
               long connfd;

               //Controllo se è una richiesta di connessione
               if (i == socket_fd && NO_MORE_CONNECTIONS == false) {
                   connfd = accept(socket_fd, (struct sockaddr *) NULL, NULL);
                   if(connfd == -1)
                       continue;

                   printf("Client %ld connesso\n", connfd);
                   connected_clients++;
                   FD_SET(connfd, &set);
                   if (connfd > fdmax)
                       fdmax = (int)connfd;
                   continue;
               }

               //Controllo se è la pipe di un thread che mi comunica di aver finito la gestione di una richiesta
               //così posso rimettere tra i descrittori da analizzare il client
               if (i == pfd[0]) {
                   int client_to_add;
                   if (read(pfd[0], &client_to_add, sizeof(int)) > 0) {
                       //printf("Il client da riaggiungere e' %d\n", client_to_add);
                       FD_SET(client_to_add, &set);
                       if (client_to_add > fdmax)
                           fdmax = client_to_add;
                       continue;
                   } else {
                       printf("Errore lettura pipe\n");
                       continue;
                   }
               }
               //Se è il thread dei segnali che mi dice di svegliarmi lo ignoro
               if (i == signal_pipe[0]){
                   continue;
               }

               //Altrimenti è una richiesta da parte di un client
               connfd = i;
               int opcode = 0;
               //Leggo il codice operazione
               if (readn(connfd, &opcode, sizeof(int)) <= 0) {
                   //Se c'è un errore nella lettura chiudo la connessione con il client
                   printf("Client %ld disconnesso\n", connfd);
                   connected_clients--;
                   FD_CLR(connfd, &set);
                   close((int)connfd);
                   continue;
               }
               //Se è una SHUTDOWN del server
               if (opcode == SHUTDOWN) {
                   FD_CLR(connfd, &set);
                   close((int)connfd);
                   goto cleanup;
               }

               //Creo una nuova richiesta
               req = (request_t *) malloc(sizeof(request_t));
               if(req == NULL){
                   perror("Errore fatale malloc");
                   exit(EXIT_FAILURE);
               }
               req->opcode = opcode;
               req->client_fd = connfd;
               req->storage = myStorage;
               req->pipe_write = pfd[1];

               //Se la richiesta non è una shutdown allora è una richiesta da gestire nel pool
               if (addToThreadPool(workers,(void *) request_handler_function,req) == -1) {
                   //Se c'è un errore col threadpool comunico fallimento al client
                   int error = FAILURE;
                   writen(connfd, &error, sizeof(int));
                   continue;
               }
               //printf("Richiesta inviata al thread\n");
               //Rimuovo il client dal set perché adesso viene gestito dal nuovo thread
               FD_CLR(connfd, &set);
               if (connfd == fdmax)
                   fdmax = updatemax(set, fdmax);
           }
       }
     }

     //FASE FINALE DI CLEANUP
     cleanup:
     if(workers)
        destroyThreadPool(workers, 0);
     if(myStorage)
        storage_destroy(myStorage);
     if(socket_fd != -1)
        close(socket_fd);
     if(signal_thread_activated)
         pthread_kill(signal_thread, SIGKILL);
     pthread_join(signal_thread, NULL);

     //EXIT
     exit:
     if(!strcmp(NOME_SOCKET,"")) unlink(NOME_SOCKET);

     return 0;
}




/********************* FUNZIONI AGGIUNTIVE **************************/

int updatemax(fd_set set, int fdmax){
    for(int i = (fdmax-1); i >= 0; --i)
        if (FD_ISSET(i, &set)) return i;
    //assert(1==0);
    return -1;
}

//SIGINT, SIGTERM, SIGQUIT ---> NO_MORE_CONNECTIONS = true; NO_MORE_REQUESTS = true;
//SIGHUP --> NO_MORE_CONNECTIONS = true; NO_MORE_REQUESTS = false;
static void signals_checker(void* arg){
    printf(" !   Segnali abilitati   !\n");
    sigset_t* sigset = (sigset_t*) arg;
    int signal;
    if( sigwait(sigset, &signal) != 0) exit(EXIT_FAILURE);
    //printf("Segnale\n");
        switch (signal)
        {
            case SIGINT:
            case SIGQUIT:
                NO_MORE_CONNECTIONS = true;
                NO_MORE_REQUESTS = true;
                printf("Terminazione immediata\n");
                break;

            case SIGHUP:
                NO_MORE_CONNECTIONS = true;
                NO_MORE_REQUESTS = false;
                printf("Terminazione in corso ... in attesa che tutti i client si disconnettano\n");
                break;

            case SIGPIPE:
            default:
                break;

        }
        char msg[10] = "wake up!";
    if(write(signal_pipe[1], msg, sizeof(msg)) <= 0) {
        perror("Errore scrittura pipe\n");
        exit(FATAL);
    }
}


/**
 * @brief Legge una singola linea del file di configurazione e la tokenizza rispetto a '='.
 * Se trova una corrispondenza aggiorna la rispettiva variabile.
 *
 * @param line -- linea del file da tokenizzare
 * @param NOME_SOCKET -- puntatore al nome del socket del server
 * @param NUMERO_WORKERS -- puntatore al numero di thread workers del server
 * @param LIMITE_FILE -- puntatore al numero massimo di file del server
 * @param LIMITE_MB -- puntatore al numero massimo di MB disponibili nel server
 * @param METODO_RIMPIAZZAMENTO -- puntatore al tipo di metodo di rimpiazzamento del server
 *
 * @returns 0 se non ci sono errori, -1 in caso di errore
 */
int read_line(char *line, char* NOME_SOCKET, int* NUMERO_WORKERS,
              int* LIMITE_FILE, int* LIMITE_MB, int* METODO_RIMPIAZZAMENTO) {

    char *ptr;
    long n = 0;
    char *tmpstr;
    char *token = strtok_r(line, "=", &tmpstr);

    //Imposta il nome del socket
    if (strcmp(token, "SOCKET_NAME") == 0) {
        token = strtok_r(NULL, "=", &tmpstr);
        if (!token || *token == '\n') {
            printf("Socketname mancante\n");
            return -1;
        }
        if (strlen(token) >= MAX_PATH) {
            printf("Socket name troppo lungo\n");
            return -1;
        }
        token[strlen(token)-1] = '\0';
        strcpy(NOME_SOCKET, token);
        return 0;
    }

    //Imposta numero di thread workers
    if (strcmp(token, "N_WORKERS") == 0) {
        token = strtok_r(NULL, "=", &tmpstr);
        if (!token || *token == '\n') {
            printf("Numero di thread workers mancante\n");
            return -1;
        }
        if (isNumber(token, &n) != 0 || n <= 0) {
            printf("Input file configurazione non valido\n");
            return -1;
        }
        if(n > INT_MAX)
            *NUMERO_WORKERS = INT_MAX;
        else
            *NUMERO_WORKERS = (int)n;
        return 0;
    }

    //Imposta limite massimo di file dello storage
    if (strcmp(token, "FILE_CAPACITY") == 0) {
        token = strtok_r(NULL, "=", &tmpstr);
        if (!token || *token == '\n') {
            printf("Quantità file massima mancante\n");
            return -1;
        }
        if (isNumber(token, &n) != 0 || n <= 0) {
            printf("Input file configurazione non valido\n");
            return -1;
        }
        if(n > INT_MAX)
            *LIMITE_FILE = INT_MAX;
        else
            *LIMITE_FILE = (int)n;
        return 0;
    }

    //Imposta capacità memoria dello storage
    if (strcmp(token, "MB_CAPACITY") == 0) {
        token = strtok_r(NULL, "=", &tmpstr);
        if (!token || *token == '\n') {
            printf("Capacità in MB mancante\n");
            return -1;
        }
        if (isNumber(token, &n) != 0 || n <= 0) {
            printf("Input file configurazione non valido\n");
            return -1;
        }
        long mb = strtol(token, &ptr, 10);
        if(mb > INT_MAX)
            *LIMITE_MB = INT_MAX;
        else
            *LIMITE_MB = (int)n;
        return 0;
    }

    //Imposta politica di rimpiazzamento
    //TODO controllare i '\n' e decidere cosa fare
    if (strcmp(token, "REPLACE_MODE") == 0) {
        token = strtok_r(NULL, "=", &tmpstr);
        if (!token || *token == '\n') {
            printf("Modalità di rimpiazzamento mancante\n");
            return -1;
        }
        if ( (strcmp(token, "FIFO") == 0) || (strcmp(token,"FIFO\n") == 0) ){
            *METODO_RIMPIAZZAMENTO = 0;
            return 0;
        }
        if (strcmp(token, "LRU") == 0) {
            *METODO_RIMPIAZZAMENTO = 1;
            return 0;
        }
        printf("Modalità di rimpiazzamento non valida, modalità disponibili: FIFO, LRU\n");
        return -1;
    }
    return -1;}


/**
 * @brief Apre e legge il file di configurazione e inizializza i parametri.
 *
 * @param config_filename -- nome del file
 * @param NOME_SOCKET -- puntatore al nome del socket del server
 * @param NUMERO_WORKERS -- puntatore al numero di thread workers del server
 * @param LIMITE_FILE -- puntatore al numero massimo di file del server
 * @param LIMITE_MB -- puntatore al numero massimo di MB disponibili nel server
 * @param METODO_RIMPIAZZAMENTO -- puntatore al tipo di metodo di rimpiazzamento del server
 *
 * @returns 0 se non ci sono errori, -1 in caso di errore
 */
int read_config_file(char* config_filename, char* NOME_SOCKET, int* NUMERO_WORKERS,
                     int* LIMITE_FILE, int* LIMITE_MB, int* METODO_RIMPIAZZAMENTO){
    FILE *ptr;
    char* line;
    line = (char*)malloc(MAX_LINE*(sizeof(char)) +1 );
    if(!line) return -1;

    if( ( ptr =  fopen(config_filename, "r" ) ) == NULL){
        perror("Apertura file di configurazione");
        free(line);
        return -1;
    }

    while(fgets(line, MAX_LINE, ptr ) != NULL ){
        if(read_line(line, NOME_SOCKET, NUMERO_WORKERS, LIMITE_FILE, LIMITE_MB, METODO_RIMPIAZZAMENTO) == -1) {
            free(line);
            fclose(ptr);
            return -1;
        }
    }
    free(line);
    fclose(ptr);
    return 0;
}
