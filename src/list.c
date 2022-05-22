//
// Created by linuxlite on 21/02/22.
//
#include <list.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**
 * Crea una lista di tipo list_t allocando lo spazio necessario.
 * Controlla che l'allocazione sia andata a buon fine.
 *
 * @return puntatore alla lista creata o NULL in caso di errore. Sets errno
 */
list_t* list_create() {

    list_t *return_list = (list_t *)malloc(sizeof(list_t));
    if(!return_list) {
       return NULL;
    }
    return_list->HEAD = NULL;
    return_list->TAIL = NULL;
    return_list->num_elem = 0;

    return return_list;
}

void list_printAsString(list_t* list){

    if (!list || !list->HEAD)
        return;

    elem_t *elem = list->HEAD;
    while (elem != NULL) {
        printf("%s\n", (char*)elem->content);
        elem = elem->next;
    }

}


/**
 *  Aggiunge in coda alla lista puntata dal parametro list
 *  un elemento con contenuto puntato dal parametro content
 *  Controlla che l'allocazione del nuovo elemento sia andata a buon fine.
 *
 * @param list - puntatore alla lista a cui aggiungere l'elemento
 * @param content - puntatore al contenuto generico del nuovo elemento
 * @return il puntatore all'oggetto appena inserito, NULL in caso di errore, Sets errno
 */
elem_t* list_append(list_t* list, void* content){

    if(!list || !content)
        return NULL;

    elem_t* new_elem = NULL;
    if( (new_elem = (elem_t*)malloc(sizeof(elem_t))) == NULL){
        return NULL;
    }
    new_elem->content = content;
    new_elem->previous = NULL;
    new_elem->next = NULL;

    //Se la lista è vuota
    if(list->num_elem == 0) {
        list->HEAD = new_elem;
        list->TAIL = new_elem;
        list->num_elem++;
    }else{
        new_elem->previous = list->TAIL;
        list->TAIL->next = new_elem;
        list->TAIL = new_elem;
        list->num_elem++;
    }
    return new_elem;
}




/**
 *  Aggiunge in testa alla lista puntata dal parametro list
 *  un elemento con contenuto puntato dal parametro content
 *  Controlla che l'allocazione del nuovo elemento sia andata a buon fine.
 *
 * @param list - puntatore alla lista a cui aggiungere l'elemento
 * @param content - puntatore al contenuto generico del nuovo elemento
 * @return il puntatore all'oggetto appena inserito, NULL in caso di errore, Sets errno
 */
elem_t* list_prepend(list_t* list, void* content){

    if(!list || !content)
        return NULL;

    elem_t* new_elem = NULL;
    if( (new_elem = (elem_t*)malloc(sizeof(elem_t))) == NULL){
        return NULL;
    }
    new_elem->content = content;
    new_elem->next = NULL;
    new_elem->previous = NULL;

    //Se la lista è vuota
    if(list->num_elem == 0) {
        list->HEAD = new_elem;
        list->TAIL = new_elem;
        list->num_elem++;
    }else{
        new_elem->next = list->HEAD;
        list->HEAD->previous = new_elem;
        list->HEAD = new_elem;
        list->num_elem++;
    }
    return new_elem;
}




/**
 * Restituisce il puntatore al primo nodo della lista, senza rimuoverlo
 *
 * @param list - lista da cui prelevare il nodo di testa
 * @return il puntatore al primo nodo della lista, NULL in caso errore
 */
elem_t* list_get_head(list_t* list){

    if(!list || list->num_elem == 0)
        return NULL;

    return list->HEAD;
}




/**
 * Rimuove il primo nodo dalla lista senza deallocarlo.
 *
 * @param list - puntatore alla lista da cui rimuovere il primo nodo
 * @return puntatore al nodo appena rimosso
 */
elem_t* list_remove_head(list_t* list){

    if(!list || list->num_elem == 0 || (list->HEAD) == NULL)
        return NULL;

    elem_t* return_elem = list->HEAD;
    list->HEAD = list->HEAD->next;
    if(list->HEAD) list->HEAD->previous = NULL;
    list->num_elem--;

    if(list->num_elem == 0){
        list->TAIL = list->HEAD;
        if(list->HEAD) list->HEAD->next = NULL;
    }

    return return_elem;
}





/**
 * Restituisce il puntatore all'ultimo nodo della lista, senza rimuoverlo
 *
 * @param list - lista da cui prelevare il nodo di coda
 * @return il puntatore all'ultimo nodo della lista, NULL in caso errore
 */
elem_t* list_get_tail(list_t* list){

    if(!list || list->num_elem == 0)
        return NULL;

    return list->TAIL;

}





/**
 * Rimuove l'ultimo nodo dalla lista senza deallocarlo.
 *
 * @param list - puntatore alla lista da cui rimuovere l'ultimo nodo
 * @return puntatore al nodo appena rimosso
 */
elem_t* list_remove_tail(list_t* list){

    if(!list || list->num_elem == 0)
        return NULL;

    elem_t* return_elem = list->TAIL;
    list->TAIL = list->TAIL->previous;
    if(list->TAIL) list->TAIL->next = NULL;
    list->num_elem--;

    if(list->num_elem == 0){
        list->HEAD = list->TAIL;
        if(list->TAIL) list->TAIL->previous = NULL;
    }

    return return_elem;
}




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
elem_t* list_get_elem(list_t* list, void* value, int (*compare_function)(void*, void*)) {

    if (!list || !list->HEAD)
        return NULL;

    elem_t *elem = list->HEAD;
    while (elem != NULL) {
        //Se la compare_function è diversa da NULL uso quella, altrimenti uso uguaglianza semplice
        if (((compare_function != NULL) && (compare_function(elem->content, value) == 0)) ||
            ((compare_function == NULL) && (elem->content == value))) {
            //elemento trovato
            return elem;
        }
        elem = elem->next;
    }
    return NULL;
}


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
elem_t* list_remove_elem(list_t* list, void* value, int (*compare_function)(void*, void*)) {

    if (!list || !list->HEAD)
        return NULL;

    elem_t* return_elem = NULL;
    elem_t *elem = list->HEAD;
    while (elem != NULL) {
        //Se la compare_function è diversa da NULL uso quella, altrimenti uso una normale uguaglianza
        if (((compare_function != NULL) && (compare_function(elem->content, value) == 0)) ||
            ((compare_function == NULL) && (elem->content == value))) {
            //elemento trovato
            return_elem = elem;
            //Se è l'elemento di testa
            if (elem == list->HEAD) {
                list->HEAD = elem->next;
                if (list->HEAD)
                    list->HEAD->previous = NULL;
                list->num_elem--;
            }
                //Se è l'elemento di coda
            else if (elem == list->TAIL) {
                list->TAIL = elem->previous;
                if (list->TAIL)
                    list->TAIL->next = NULL;
                list->num_elem--;
            }
                //E' un elemento centrale
            else {
                elem->previous->next = elem->next;
                elem->next->previous = elem->previous;
                list->num_elem--;
            }

            //Se non ci sono più elementi sistemo la coda
            if (list->num_elem == 0) {
                list->HEAD = NULL;
                list->TAIL = NULL;
            }
            return return_elem;
        }
        elem = elem->next;
    }
    return NULL;
}

//Passato un puntatore ad un elemento restituisce elem->next, utilizzata per iterare
elem_t* list_getNext(list_t* list, elem_t* elem){
    if(list == NULL || elem == NULL) return NULL;
    else
        return elem->next;
}


/**
 * Funzione che dealloca la lista e ogni suo nodo.
 *
 * @param list -  puntatore alla lista da deallocare
 * @param free_content - puntatore alla funzione che dealloca il contenuto dei nodi,
 * può essere NULL se il contenuto non deve essere deallocato.
 */
void list_destroy(list_t* list, void (*free_content)(void*)){

    if(list == NULL)
        return;

    while(list->num_elem != 0){
       elem_t* toDestroy = list_remove_head(list);
       if(free_content && toDestroy->content)
           (free_content)(toDestroy->content);
       free(toDestroy);
    }

    if(list) {
        free(list);
        list = NULL;
    }

}