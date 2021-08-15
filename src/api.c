//
// Created by francesco on 15/08/21.
//

#include <stdio.h>
#include <time.h>
#include <aio.h>
#include <conn.h>
#include <unistd.h>
#include <util.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include "api.h"

static int sockfd = -1;

int openConnection(const char* sockname, int msec, const struct timespec abstime){

    if (sockfd != -1)
    {
        errno = EISCONN;
        return -1;
    }
    if (!sockname || strlen(sockname) > PATH_MAX || msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

 struct sockaddr_un serv_addr;
 SYSCALL_EXIT("socket", sockfd, socket(AF_UNIX, SOCK_STREAM, 0), "socket", "");
 memset(&serv_addr, '0', sizeof(serv_addr));

 serv_addr.sun_family = AF_UNIX;
 strncpy(serv_addr.sun_path,sockname, strlen(sockname)+1);

 time_t current_time = time(NULL);

 printf("Connessione al server in corso..\n");
 int error;

 while(  (error = connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) ) == -1
         && current_time <= abstime.tv_sec ){


     //TODO imposta i vari errno

     sleep(msec/1000);
     current_time = time(NULL);
 }
 if(error == 0){
     printf("Connessione effettuata\n");
     return 1;
 }
 else{
     perror("Impossibile connettersi al server"); //todo impostare tutti le uscite errori
     return -1;
 }
}