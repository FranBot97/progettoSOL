#include <list.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

elem_t* list_get_head(list_t* list){

    if(!list || list->num_elem == 0)
        return NULL;

    return list->HEAD;
}

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

elem_t* list_get_tail(list_t* list){

    if(!list || list->num_elem == 0)
        return NULL;

    return list->TAIL;
}

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

elem_t* list_getNext(list_t* list, elem_t* elem){
    if(list == NULL || elem == NULL) return NULL;
    else
        return elem->next;
}

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