#ifndef LIST_H
#define LIST_H
#include "stdlib.h"

/**  Elemento che rappresenta un nodo della lista.
 *
 *  @param next - puntatore al nodo successivo
 *  @param previous - puntatore al nodo precedente
 *  @param content - contenuto generico del nodo
 **/
typedef struct elem_{
    struct elem_* next;
    struct elem_* previous;
    void* content;
}elem_t;

/**  Lista
 *
 *  @param HEAD - puntatore al primo nodo
 *  @param TAIL - puntatore all'ultimo nodo
 *  @param num_elem - numero di nodi
 **/
typedef struct list{
    elem_t* HEAD;
    elem_t* TAIL;
    int num_elem;
}list_t;


/**
 * Crea una lista di tipo list_t allocando lo spazio necessario.
 * Controlla che l'allocazione sia andata a buon fine.
 *
 * @return puntatore alla lista creata o NULL in caso di errore. Sets errno
 */
list_t* list_create();

//Stampa ogni elemento della lista come array di caratteri
void list_printAsString(list_t* list);

/**
 *  Aggiunge in coda alla lista puntata dal parametro list
 *  un elemento con contenuto puntato dal parametro content
 *  Controlla che l'allocazione del nuovo elemento sia andata a buon fine.
 *
 * @param list - puntatore alla lista a cui aggiungere l'elemento
 * @param content - puntatore al contenuto generico del nuovo elemento
 * @return il puntatore all'oggetto appena inserito, NULL in caso di errore, Sets errno
 */
elem_t* list_append(list_t* list, void* content);

/**
 *  Aggiunge in testa alla lista puntata dal parametro list
 *  un elemento con contenuto puntato dal parametro content
 *  Controlla che l'allocazione del nuovo elemento sia andata a buon fine.
 *
 * @param list - puntatore alla lista a cui aggiungere l'elemento
 * @param content - puntatore al contenuto generico del nuovo elemento
 * @return il puntatore all'oggetto appena inserito, NULL in caso di errore, Sets errno
 */
elem_t* list_prepend(list_t* list, void* content);

/**
 * Restituisce il puntatore al primo nodo della lista, senza rimuoverlo
 *
 * @param list - lista da cui prelevare il nodo di testa
 * @return il puntatore al primo nodo della lista, NULL in caso errore
 */
elem_t* list_get_head(list_t* list);

//Restituisce l'elemento successivo di quello passato come parametro
elem_t* list_getNext(list_t* list, elem_t* elem);

/**
 * Rimuove il primo nodo dalla lista senza deallocarlo.
 *
 * @param list - puntatore alla lista da cui rimuovere il primo nodo
 * @return puntatore al nodo appena rimosso
 */
elem_t* list_remove_head(list_t* list);

/**
 * Restituisce il puntatore all'ultimo nodo della lista, senza rimuoverlo
 *
 * @param list - lista da cui prelevare il nodo di coda
 * @return il puntatore all'ultimo nodo della lista, NULL in caso errore
 */
elem_t* list_get_tail(list_t* list);

/**
 * Rimuove l'ultimo nodo dalla lista senza deallocarlo.
 *
 * @param list - puntatore alla lista da cui rimuovere l'ultimo nodo
 * @return puntatore al nodo appena rimosso
 */
elem_t* list_remove_tail(list_t* list);

/**
 * Confronta il contenuto dei nodi della lista con il valore passato come parametro,
 * se trova una corrispondenza restituisce un puntatore all'elemento senza rimuoverlo dalla lista.
 *
 * @param list - puntatore alla lista
 * @param value - valore da confrontare con il contenuto dei nodi della lista
 * @param compare_function - funzione utilizzata per confrontare i nodi,
 * deve restituire 0 in caso di successo. Può essere sostituita con NULL per utilizzare l'uguaglianza
 * semplice (n == m)
 *
 * @return puntatore all'elemento trovato, NULL altrimenti
 */
elem_t* list_get_elem(list_t* list, void* value, int (*compare_function)(void*, void*));

/**
 * Confronta il contenuto dei nodi della lista con il valore passato come parametro,
 * se trova una corrispondenza restituisce un puntatore all'elemento rimuovendolo dalla lista,
 * ma senza deallocarlo.
 *
 * @param list - puntatore alla lista
 * @param value - valore da confrontare con il contenuto dei nodi della lista
 * @param compare_function - funzione utilizzata per confrontare i nodi,
 * deve restituire 0 in caso di successo. Può essere sostituita con NULL per utilizzare l'uguaglianza
 * semplice (n == m)
 *
 * @return puntatore all'elemento trovato, NULL altrimenti
 */
elem_t* list_remove_elem(list_t* list, void* value, int (*compare_function)(void*, void*));

/**
 * Funzione che dealloca la lista e ogni suo nodo.
 *
 * @param list -  puntatore alla lista da deallocare
 * @param free_content - puntatore alla funzione che dealloca il contenuto dei nodi,
 * può essere NULL se il contenuto non deve essere deallocato.
 */
void list_destroy(list_t* list, void (*)(void*));

#endif //LIST_H
