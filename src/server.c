
#include <storage.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <stdbool.h>
#include <threadpool.h>
#include <request.h>
#include <request_handler.h>
#include <util.h>
#include <signal.h>

#define MAX_PATH 108
#define LINE_MAX 128
#define MAX_CONN 20

/*calcol maxfd*/
int updatemax(fd_set set, int fdmax){
    for(int i = (fdmax-1); i >= 0; --i)
        if (FD_ISSET(i, &set)) return i;
    //assert(1==0);
    return -1;
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
        *NUMERO_WORKERS = n;
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
        *LIMITE_FILE = n;
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
        *LIMITE_MB = strtol(token, &ptr, 10);
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
    line = (char*)malloc(LINE_MAX*(sizeof(char)) +1 );
    if(!line) return -1;

    if( ( ptr =  fopen(config_filename, "r" ) ) == NULL){
        perror("Apertura file di configurazione");
        free(line);
        return -1;
    }

    while(fgets(line, LINE_MAX, ptr ) != NULL ){
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


static void* signals_check(void* write_fd){

    int writefd = *(int*)write_fd;
    printf("%d test\n", writefd);
    return NULL;
}



int main(int argc, char* argv[]) {

    printf("Server avviato\n");
    //printf("%d\n", argc);
    if(argc < 3){
        printf("Esegui come: server -f <nome_file.txt>\n");
        goto exit;
    }

    /*****VARIABILI SERVER******/
    char NOME_SOCKET[MAX_PATH] = "";
    int NUMERO_WORKERS = 0;
    int MAX_FILE = 0;
    int MAX_MEMORIA = 0;
    int REPLACE_MODE = 0; //0 = FIFO, 1 = LRU
    //pthread_mutex_t file_storage;
    //pthread_mutex_t files_mutex;
    threadpool_t* workers = NULL;
    storage_t* myStorage = NULL;
    int pfd[2]; //pipe
    sigaction(SIGPIPE, &(struct sigaction){SIG_IGN}, NULL);
    //signal(SIGINT, )

    /*******THREAD GESTORE SEGNALI***********/
    pthread_t signal_tid;
    int err;
    int signals[2];
    if(pipe(signals) == -1){
        printf("Errore nella creazione della pipe\n");
        goto cleanup;
    }
    printf("%d e %d \n", signals[0], signals[1]);
    //signals[0] per lettura
    //signals[1] scrittura
    err = pthread_create(&signal_tid, NULL, signals_check, (void*)&signals[1] );
    if(err != 0)
        goto cleanup;
    /***** FINE VARIABILI SERVER *****/

    /**** LETTURA FILE ****/
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
                 printf("Comando non riconosciuto\n");
                 goto cleanup;

             default:
                 printf("Comando non riconosciuto\n");
                 goto cleanup;
         }
     }

     /*** INIZIALIZZAZIONE DOPO LETTURA FILE ***/
     if(pipe(pfd) == -1){
         printf("Errore nella creazione della pipe\n");
         goto cleanup;
     }
     //pfd[0] per lettura
     //pfd[1] scrittura

     myStorage = storage_create(MAX_FILE, MAX_MEMORIA, REPLACE_MODE);
     if(myStorage == NULL) {
         printf("Errore nella creazione del file storage\n");
         goto cleanup;
     }else{
         printf("file storage creato correttamente\n");
         //goto clean;
     }

     workers = createThreadPool(NUMERO_WORKERS, 10);
     if(workers == NULL){
         printf("Errore nella creazione del threadpool\n");
         goto cleanup;
     }else{
         printf("Threadpool creato correttamente\n");
         //goto clean;
     }

   //pthread_mutex_init(&file_storage, NULL); //TODO check errors
   // pthread_mutex_init(&files_mutex, NULL); //TODO check

    //INIZIO CREAZIONE SOCKET
    int socket_fd;
    unlink(NOME_SOCKET);
    if ( (socket_fd = socket(AF_UNIX, SOCK_STREAM,0) ) == -1){
        perror("Creazione socket");
        goto cleanup;}
    struct sockaddr_un serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, NOME_SOCKET, MAX_PATH);
   if ( bind(socket_fd, (struct sockaddr*)&serv_addr,sizeof(serv_addr)) == -1){
       perror("Bind fallita");
       goto cleanup;}
   if(  listen(socket_fd, MAX_CONN)  == -1 ){
       perror("Listen fallita");
       goto cleanup;}
    printf("Socket creata\n");
   //FINE CREAZIONE SOCKET

   //SELECT
   fd_set set, rdset;
   FD_ZERO(&set);
   FD_ZERO(&rdset);
   FD_SET(socket_fd, &set);
   FD_SET(pfd[0], &set);
   int fdmax = (socket_fd > pfd[0] ? socket_fd : pfd[0]);

   while(true){
       rdset = set;

       if(select(fdmax+1, &rdset, NULL, NULL, NULL) == -1){
           perror("select");
           goto cleanup;
       }
       //esamino i fd
       for(int i=0; i <= fdmax; i++){

           //Se c'è una richiesta di lettura
           if(FD_ISSET(i, &rdset)) {
               int connfd;

               //Controllo se è una richiesta di connessione
               if (i == socket_fd) {
                   connfd = accept(socket_fd, (struct sockaddr *) NULL, NULL);
                   printf("Nuovo client %d connesso\n", connfd);
                   FD_SET(connfd, &set);
                   if (connfd > fdmax)
                       fdmax = connfd;
                   continue;
               }

               //Controllo se è la pipe
               else if (i == pfd[0]) {
                   int client_to_add;
                   if (read(pfd[0], &client_to_add, sizeof(int)) > 0) {
                       printf("Il client da riaggiungere e' %d\n", client_to_add);
                       FD_SET(client_to_add, &set);
                       if (client_to_add > fdmax)
                           fdmax = client_to_add;
                       continue;
                   } else {
                       printf("Errore lettura pipe\n");
                       continue;
                   }

               } else{
                   //Altrimenti è una richiesta da parte di un client, assegno il task ad un thread
                   connfd = i;
                   int len = 0;

                   if (readn(connfd, &len, sizeof(int)) <= 0) {
                       FD_CLR(connfd, &set);
                       close(connfd);
                       continue;
                   }

                   //Creo una nuova richiesta
                   request_t *req = (request_t *) malloc(sizeof(request_t)); //TODO check
                   req->command = NULL;
                   req->len = len;
                   req->client_fd = connfd;
                //   req->storage_mutex = &file_storage;
               //    req->files_mutex = &files_mutex;
                   req->myStorage = myStorage;
                   req->pipe_write = pfd[1];

                   printf("Nuova richiesta lunga %d\n", req->len);

                   //req->command = (char *) malloc(req->len * sizeof(char)+1);
                   req->command = calloc(len, sizeof(char)+1);
                   if (req->command == NULL)
                       printf("error malloc\n");
                  // memset(req->command, '0', sizeof(char)*req->len);

                   if (readn(connfd, req->command, req->len * sizeof(char)) <= 0) {
                       close(connfd);
                       FD_CLR(connfd, &set);
                       printf("Errore lettura richiesta\n");
                       if (connfd == fdmax)
                           fdmax = updatemax(set, fdmax);
                   } else {
                       printf("RICHIESTA RICEVUTA %s\n", req->command);
                       //TODO finchè non gestisco i segnali uso quit
                       //FINCHE' NON USO SEGNALI USO "QUIT" COME PAROLA DI TERMINAZIONE SERVER
                       //INVIATA DA UN CLIENT
                       if (strcmp(req->command, "quit") == 0) {
                           printf("RICHIESTA RICEVUTA %s\n", req->command);
                           FD_CLR(connfd, &set);
                           close(connfd);
                           if (req) {
                               if (req->command) {
                                   free(req->command);
                                   req->command = NULL;
                               }
                               free(req);
                               req = NULL;
                           }
                           goto cleanup;
                      //     continue;

                       }

                       //Se la richiesta non è una quit allora è una richiesta da gestire nel pool
                       if (addToThreadPool(workers,(void *) request_handler_function,req) == -1) {
                           //Se c'è un errore col threadpool
                           writen(connfd, "Servizio non disponibile\n", 26);
                           continue;
                       };
                       printf("Richiesta inviata al thread\n");
                       //Rimuovo il client dal set perché adesso viene gestito dal nuovo thread
                       FD_CLR(connfd, &set);
                       if (connfd == fdmax)
                           fdmax = updatemax(set, fdmax);
                   }
               }
           }
       }
   }
   //FINE SELECT

    /*** INIZIO PULIZIA MEMORIA ***/
    cleanup:
    if(myStorage)
        storage_destroy(myStorage);
    if(workers)
        destroyThreadPool(workers, 0);
    pthread_join(signal_tid, NULL);

    /***  FINE PULIZIA MEMORIA ***/
    exit:
    if(!strcmp(NOME_SOCKET,"")) unlink(NOME_SOCKET);
    return 0;
}
