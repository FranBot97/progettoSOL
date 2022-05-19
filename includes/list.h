//
// Created by linuxlite on 21/02/22.
//

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

list_t* list_create();

void list_printAsString(list_t* list);

elem_t* list_append(list_t* list, void* content);

elem_t* list_prepend(list_t* list, void* content);

elem_t* list_get_head(list_t* list);

elem_t* list_getNext(list_t* list, elem_t* elem);

elem_t* list_remove_head(list_t* list);

elem_t* list_get_tail(list_t* list);

elem_t* list_remove_tail(list_t* list);

elem_t* list_get_elem(list_t* list, void* value, int (*compare_function)(void*, void*));

elem_t* list_remove_elem(list_t* list, void* value, int (*compare_function)(void*, void*));

void list_destroy(list_t* list, void (*)(void*));

#endif //LIST_H
