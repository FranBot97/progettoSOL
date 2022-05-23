#ifndef UTIL_H
#define UTIL_H
/* File contenente funzioni di utilità e costanti.
 * Visibile, per semplicità, sia lato client che lato server.
 * */

#include <errno.h>
#include <icl_hash.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_FILENAME 128 //Lunghezza massima nomi file
#define MAX_SOCKNAME 107 //Lunghezza massima nome socket
#define MAX_ARGLEN 1024 //Lunghezza massima argv
#define MAX_CONN 20 //Numero massimo di connessioni
#define MAX_PATH MAX_FILENAME+MAX_ARGLEN //Massimo percorso file
#define MAX_LINE 128 //Massima lunghezza di una riga di file
#define MAX_STRLEN 128 //Lunghezza generica array di caratteri
#define RETRY_CONN_MSEC 2000 //Tempo in msec tra ogni tentativo di riconnessione
#define TIMEOUT_CONN_SEC 10 //Tempo di timeout in secondi, dopo TIMEOUT_CONN_SEC non si ritenta più la connessione
#define ERROR_STRLEN MAX_PATH*2 //Lunghezza stringa per eventuali args di errori

/** Evita letture parziali
 *
 *   \retval -1   errore (errno settato)
 *   \retval  0   se durante la lettura da fd leggo EOF
 *   \retval size se termina con successo
 */
static inline int readn(long fd, void *buf, size_t size) {
    size_t left = size;
    int r;
    char *bufptr = (char*)buf;
    while(left>0) {
	if ((r=read((int)fd ,bufptr,left)) == -1) {
	    if (errno == EINTR) continue;
	    return -1;
	}
	if (r == 0) return 0;   // EOF
        left    -= r;
	bufptr  += r;
    }
    return size;
}

/** Evita scritture parziali
 *
 *   \retval -1   errore (errno settato)
 *   \retval  0   se durante la scrittura la write ritorna 0
 *   \retval  1   se la scrittura termina con successo
 */
static inline int writen(long fd, void *buf, size_t size) {
    size_t left = size;
    int r;
    char *bufptr = (char*)buf;
    while(left>0) {
	if ((r=write((int)fd ,bufptr,left)) == -1) {
	    if (errno == EINTR) continue;
	    return -1;
	}
	if (r == 0) return 0;  
        left    -= r;
	bufptr  += r;
    }
    return 1;
}

/**
 * @brief Controlla se la stringa passata come primo argomento e' un numero.
 * @return  0 ok,  1 non e' un numero, 2 overflow/underflow
 */
static inline int isNumber(const char* s, long* n) {
    if (s == NULL) return 1;
    if (strlen(s)==0) return 1;
    char* e = NULL;
    errno=0;
    long val = strtol(s, &e, 10);
    if (errno == ERANGE) return 2;    // overflow/underflow
    if (e != NULL && (*e == (char)0 || *e=='\n') ) {
        *n = val;
        return 0;   // successo
    }
    return 1;   // non e' un numero
}

/**
 * @brief Preleva il nome del file dal percorso passato.
 * @param pathname - percorso del file
 * @param result - stringa dove viene memorizzato il risultato
 */
static inline void parseFilename(char* pathname, char* result){
    char* filename;
    char name[MAX_FILENAME];
    char* rest = NULL;
    filename = strtok_r(pathname, "/", &rest);
    while(filename != NULL){
        strcpy(name, filename);
        filename = strtok_r(NULL, "/", &rest);
    }
    strcpy(result, name);
}


/** Funzione extra per tabella hash **/
static inline unsigned int fnv_hash_function( void *key, int len ) {
    unsigned char *p = (unsigned char*)key;
    unsigned int h = 2166136261u;
    int i;
    for ( i = 0; i < len; i++ )
        h = ( h * 16777619 ) ^ p[i];
    return h;
}

/* funzioni hash per per l'hashing di interi */

static inline unsigned int ulong_hash_function( void *key ) {
    int len = sizeof(unsigned long);
    unsigned int hashval = fnv_hash_function( key, len );
    return hashval;
}

static inline int ulong_key_compare( void *key1, void *key2  ) {
    return ( *(unsigned long*)key1 == *(unsigned long*)key2 );
}

/* Macro usata per stampa nell'API */
#define PRINT_INFO(condition, ...) \
	if (condition) { fprintf(stdout, __VA_ARGS__); \
    fflush(stdout);}

#endif
