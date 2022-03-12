//
// Created by francesco on 19/11/21.
//

#ifndef FILE_H
#define FILE_H
#include <constant_values.h>

typedef struct file{

    char nome_file[MAX_FILENAME];
    int dimensione_file; //in KB
    void* contenuto_file;
    int client_lock; //client che ha acquisito la lock

}file_t;

/**
 * Crea, alloca e inizializza un nuovo file.
 *
 * //@param nome_file -- il nome del file da assegnare
 * @param dimensione_file -- la dimensione del file da assegnare
 * @param contenuto_file -- puntatore al contenuto del file
 * @param client_lock -- il file descriptor del client che ha acquisito la lock (-1 default)
 *
 * @returns puntatore al file creato, NULL in caso di errore.
 */
file_t* file_create(char* nome_file, int dimensione_file,  void* contenuto_file, int client_lock);


/**
 * Dealloca il file individuato dal puntatore e lo setta a NULL
 *
 * @param myfile -- il puntatore al file da eliminare
 *
 */
void file_destroy(file_t* myfile);


/**
 * Dealloca i campi del file e li setta a NULL, poi dealloca anche il puntatore
 *
 * @param myfile -- il puntatore al file da resettare
 *
 */
void file_clean(file_t *myfile);


#endif //FILE_H
