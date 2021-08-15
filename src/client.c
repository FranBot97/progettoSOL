//
// Created by francesco on 10/08/21.
//
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <icl_hash.h>
#include <conn.h>
#include <util.h>
#include <getopt.h>
#include <api.h>
#include <time.h>


//Tipi di richieste
enum request_type{
    empty,
    connect_op,     //-f
    sendDir_op,     //-w
    sendFile_op,    //-W
    readFile_op,    //-r
    readN_op,        //-R
    lock_op,        //-l
    unlock_op,      //-u
    remove_op,      //-c
};

/*Formato di una richiesta
 *  id: numero identificativo della richiesta lato client
 *  op: tipo di operazione
 *  args: argomenti dell'operazione
 *  dir: eventuale nome della cartella derivata dalle opzioni -d e -D
 *  next: prossima richiesta nella lista
*/
typedef struct req{

    int id;
    enum request_type op;
    char* args;
    char* dir;
    struct req* next;

}request;

/*
 * Funzione: createRequest
 * -----------------------
 *      Crea e restituisce una nuova richiesta
 *
 *      rt: tipo della richiesta da creare
 *      id: id della richiesta
 *      args: eventuali parametri (può essere NULL)
 *
 *      returns: se va a buon fine un puntatore alla richiesta appena creata
 *               altrimenti NULL
 */
request* createRequest(enum request_type rt, int id, const char* args){

    request* new_request;
    if( (new_request =  (request*)malloc(sizeof(request)) ) == NULL ) //TODO: ERRORE
             { printf("Malloc error"); return NULL;}//errore malloc

     char* request_args;
    if(args != NULL){
        if( (request_args = (char*)malloc((strlen(args)*sizeof(char)) +1)  )== NULL ) //TODO: ERRORE
        { printf("Malloc error"); return NULL;}//errore malloc
    }

    new_request->id = id;
    new_request->op = rt;
    new_request->next = NULL;
    new_request->dir = NULL;

    if(args != NULL) {
        strcpy(request_args, args);
        new_request->args = request_args;
    } else { new_request->args = NULL; }

    return new_request;

}

/*
 * Funzione: freeReq
 * Libera la memoria relativa ad una richiesta se non è NULL
 * -----------------------
 *      r: richiesta da liberare
 */
void freeReq(request* r){

    if(r->args != NULL) free(r->args);
    if(r->dir != NULL) free(r->dir);
    free(r);

}

typedef struct {

    int n_elem;
    request* first;
    request* last;

}listOfReq;

void addRequest(listOfReq* list,enum request_type rt, int id, const char* args ){

    request* tmp = NULL;
    tmp = createRequest(rt, id, args);

    if(list->first == NULL){
        list->first = tmp;
        list->last = tmp;
    } else {
        request* iter = list->first;

        while(iter->next != NULL){
            iter = iter->next;
        }

        iter->next = tmp;
        list->last = tmp;
    }

    list->n_elem++;

}

request* popRequest(listOfReq* list){

    if(list->n_elem == 0)
        return NULL;
    else{
        list->n_elem--;
        request* tmp = list->first;
        list->first = list->first->next;
        if(list->n_elem == 0){ list->last = NULL;}
        return tmp;
    }

}

void modifyLast(listOfReq* list, char* dirname){

    if( ( (list->last->dir) = (char*)malloc(strlen(dirname)*(sizeof(char))+1) ) == NULL ){ //gestione errore MALLOC
    return;}
    strcpy(list->last->dir, dirname);

}

void cleanRequests(listOfReq* list){

    request* delete;

    while(list->n_elem > 0){

        delete = popRequest(list);
        freeReq(delete);
    }
}

enum request_type checkLast(listOfReq* list){

    if(list->last != NULL)
        return list->last->op;
    else
        return empty;

}

//Stampa comandi disponibili
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

//Debug
void printReq(listOfReq* list){

    request* iter = list->first;

    while(iter != NULL){

        printf("( Type: %d ID: %d DIR: %s)------->", iter->op, iter->id, iter->dir);
        iter = iter->next;

    } printf("NULL\n");

}

int main(int argc, char* argv[]){

    if(argc < 2) {printf("Errore almeno un argomento"); return -1;} //errore argc// //TODO: ERRORE

    listOfReq* requests_to_do;  //lista di richieste inviate dal server

    if( (requests_to_do = ( (listOfReq*)malloc((sizeof(listOfReq))) )) == NULL ) //TODO: ERRORE
        { printf("Malloc error"); return -1;}//errore malloc

     /*inizializzazione lista*/
     requests_to_do->n_elem = 0;
     requests_to_do->first = NULL;
     requests_to_do->last = NULL;

     /*variabili utili*/
    char sockname[PATH_MAX];    //nome del socket
    bool print_info = false;    //bool per stampe abilitate con -p
    int error = 0;              //variabile per errori generati dalle funzioni
    enum request_type req_type; //tipo di una richiesta

    /*variabili per le richieste*/
    int id = 0;                 //id di partenza per le richieste
    int opt;                    //opt per getopt
    time_t waiting_time= 0;     //tempo tra l'invio di una richiesta e l'altra

    //booleani per verificare la ripetizione dei comandi
    //ripetizioni di comandi non ripetibili vengono ignorate (-h -f -p)
    bool h_opt = false;
    bool f_opt = false;
    bool p_opt = false;

    /*Inizio gestione input*/
    while((opt = getopt(argc, argv, "hf:w:W:D:r:R::d:t:l:u:c:p" )) != -1){

        switch(opt){

            case 'h':
                if(!h_opt) print_help();    //evita stampe multiple
                h_opt = true;
                break;

            case 'f':
                if(!f_opt){        //evita connessioni multiple
                    strncpy(sockname, optarg, PATH_MAX-1);
                    addRequest(requests_to_do, connect_op,++id, optarg);
                    f_opt = true;
                }
                break;

            case 'w':
                addRequest(requests_to_do, sendDir_op,++id, optarg);
                //invia file dalla cartella
                break;

            case 'W':
                addRequest(requests_to_do, sendFile_op,++id, optarg);
                //invia file specificati
                break;

            case 'D':
                //controllo il tipo dell'ultima richiesta
                req_type = checkLast(requests_to_do);

                //se non è -w o -W fallisce
                if(req_type != 2 && req_type != 3 ) {

                    printf("Nessuna operazione -w o -W associata\n");
                    return -1;//TODO: ERRORE
                    /*errore esci*/}
                else{
                    modifyLast(requests_to_do, optarg);
                }
                break;

            case 'r':
                addRequest(requests_to_do, readFile_op, ++id, optarg );
                //leggi i file specificati
                break;

            case 'R':
                addRequest(requests_to_do, readN_op, ++id, optarg);
                //leggi n file qualsiasi
                break;

            case 'd':
                req_type = checkLast(requests_to_do);//Se l'ultimo comando non era -r o -R
                if( req_type != 4 && req_type != 5 ) {
                    printf("Nessuna operazione -r o -R associata\n");
                    return -1;//TODO: ERRORE
                    /*errore esci*/} else{
                    //imposta cartella di raccolta file
                    modifyLast(requests_to_do, optarg);
                }
                //imposta cartella dove scrivere i file letti da -r o -R
                break;

            case 't':
                if(time_between_requests) {}; //TODO SET TIME OPTARG
                //imposta tempo tra due richieste
                break;

            case 'l':
                //lock sui file specificati
                addRequest(requests_to_do, lock_op, ++id, optarg);
                break;

            case 'u':
                //unlock sui file specificati
                addRequest(requests_to_do, unlock_op, ++id, optarg);
                break;

            case 'c':
                //cancella i file specificati dal server
                addRequest(requests_to_do, remove_op, ++id, optarg);
                break;

            case 'p':
                //abilita le stampe per le operazioni
                if(!p_opt){
                    print_info = true;
                    p_opt = true;
                    if(print_info){}
                }
                break;

        }

    }

    /*Stampa debug*/
    printReq(requests_to_do);

    /* Invio delle richieste e gestione tramite API*/

    time_t today_seconds = time(NULL);
    struct timespec limit;
    limit.tv_sec = today_seconds + TIMEOUT_LIMIT_SEC;

    //TODO ERRORE NULL
    while(requests_to_do->first != NULL){

        switch(requests_to_do->first->op){

            case empty:
                break;

            case connect_op:
                if( ( error = openConnection(requests_to_do->first->args, RETRY_TIME_MS, limit) ) == -1)
                {
                    goto clean;
                };
                break;

            case sendDir_op:
                break;

            case sendFile_op:
                break;

            case readFile_op:
                break;

            case readN_op:
                break;

            case lock_op:
                break;

            case unlock_op:
                break;

            case remove_op:
                break;

        }
       freeReq(popRequest(requests_to_do));
       printf("Prossima richiesta\n");
     // sleep()
    }


    /* Clean finali */
    clean:    cleanRequests(requests_to_do);
              free(requests_to_do);

    return error;
}

