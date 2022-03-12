//
// Created by linuxlite on 09/03/22.
//

#ifndef CONSTANT_VALUES_H
#define CONSTANT_VALUES_H
//Costanti comuni utilizzate per semplicità da tutti gli altri file

#define MAX_FILENAME 128 //Lunghezza massima nomi file
#define MAX_SOCKNAME 107 //Lunghezza massima nome socket
#define MAX_ARGLEN 1024 //Lunghezza massima argv
#define MAX_CONN 20 //Numero massimo di connessioni
#define RETRY_CONN_MSEC 2000 //Tempo in msec tra ogni tentativo di riconnessione
#define TIMEOUT_CONN_SEC 10 //Tempo di timeout in secondi, dopo TIMEOUT_CONN_SEC non si ritenta più la connessione

#endif //CONSTANT_VALUES_H
