//
// Created by francesco on 19/11/21.
//
//TODO scegliere come fare per mandare messaggi/contenuti file
//decidere protocollo client/server da utilizzare

/***** INIZIO TEST ****/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>
#include <signal.h>
#include <API.h>
#include <time.h>
#include <bits/types/struct_timeval.h>

#define MAX_PATH 108
#define MAX_LEN 107
#define LINE_MAX 128
#define MAX_CONN 20
#define RETRY_CONN_MSEC 2000
#define TIMEOUT_CONN_SEC 10
#define MAX_FILESIZE 1024

static int socket_fd;
char sockname[MAX_LEN];

enum flags_   {
    O_CREATE = 1 << 0,
    O_LOCK  = 1 << 1
};

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

int main(int argc, char* argv[]){

    struct timespec timeLimit;
    clock_gettime(CLOCK_REALTIME, &timeLimit);
    timeLimit.tv_sec += TIMEOUT_CONN_SEC;

    strncpy(sockname, "mysock", MAX_LEN);

    if (openConnection("mysock", RETRY_CONN_MSEC, timeLimit) != 0){
        perror("Connessione al server");
        return -1;
    }

    signal(SIGINT, intHandler);
    signal(SIGTSTP, intHandler);
    //printf("%ld\n", timeLimit.tv_sec);


   char request[LINE_MAX];
    if (openFile("miofile", O_CREATE) == 0)
        printf("File aperto correttamente\n");
    else
        printf("Errore nell'apertura del file\n");


    if (openFile("miofile1", O_CREATE) == 0)
        printf("File aperto correttamente\n");
    else
        printf("Errore nell'apertura del file\n");


    if (openFile("miofile2", O_CREATE | O_LOCK) == 0)
        printf("File aperto correttamente\n");
    else
        printf("Errore nell'apertura del file\n");

    if (openFile("miofile4", O_CREATE | O_LOCK) == 0)
        printf("File aperto correttamente\n");
    else
        printf("Errore nell'apertura del file\n");

    strcpy(request, "quit");
    sendToServer(request, 4, "string");




    /*if (read(socket_fd, request, LINE_MAX) <= 0) {
        perror("reading mex");
        return -1;
    };
    printf("Il mex ricevuto è: %s\n", request);

   // unlink("mysock");
    char NOME_SOCKET[MAX_PATH];
    strcpy(NOME_SOCKET,"mysock");
    socket_fd = socket(AF_UNIX,SOCK_STREAM,0);
    if(socket_fd == -1) return -1;
    struct sockaddr_un serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, NOME_SOCKET, MAX_PATH);
   if ( connect(socket_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr) ) != 0 ) {
       printf("Conn %s",serv_addr.sun_path);
       perror("");
       return -1;
   }
    printf("Connesso\n");
    fflush(stdout);
    */


  /*  while(1) {
        strcpy(request, "");
        if ( fgets(request, LINE_MAX, stdin) == NULL)
            continue;

        /**COME FARE RICHIESTE CLIENT**/
  /*
        unsigned long l = strlen(request);
        //Rimuovo '\n'
        request[l-1] = '\0';
        if (write(socket_fd, &l, sizeof(int)) < 0) {
            perror("errore w");
        }
        if (write(socket_fd, request, l * sizeof(char)) < 0) {
            perror("write client");
            return -1;
        };

       if( strcmp(request, "quit") == 0) {
        close(socket_fd);
        return 0;
       }

        printf("In attesa di risposta..\n");
        fflush(stdout);

        if (read(socket_fd, request, LINE_MAX) <= 0) {
            perror("reading mex");
            return -1;
        };
        printf("Il mex ricevuto è: %s\n", request);
    }*/
}
/***** FINE TEST ****/

