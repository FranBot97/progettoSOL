//
// Created by linuxlite on 21/02/22.
//
#include "../includes/list.h"
#include <stdlib.h>
#include <stdio.h>


list_t* list_create() {

    list_t *return_list = (list_t *) malloc(sizeof(list_t));
    if(!return_list) {
        perror("Malloc");
        return NULL;
    }
    return_list->HEAD = NULL;
    return_list->TAIL = NULL;
    return_list->num_elem = 0;

    return return_list;
}

int add_tail_element(list_t* list, void* content){

    if(!list || !content)
        return -1;

    elem_t* new_elem;
    if( (new_elem = (elem_t*)malloc(sizeof(elem_t))) == NULL)
        return -1;
    new_elem->content = content;
    new_elem->previous = NULL;
    new_elem->next = NULL;

    //Se la lista è vuota
    if(list->num_elem == 0) {
        list->HEAD = new_elem;
        list->TAIL = new_elem;
        list->num_elem++;
        return 0;
    }
    new_elem->previous = list->TAIL;
    list->TAIL->next = new_elem;
    list->TAIL = new_elem;
    list->num_elem++;

    return 0;

}

int add_head_element(list_t* list, void* content){

    if(!list || !content)
        return -1;

    elem_t* new_elem;
    if( (new_elem = (elem_t*)malloc(sizeof(elem_t))) == NULL)
        return -1;
    new_elem->content = content;
    new_elem->next = NULL;
    new_elem->previous = NULL;

    //Se la lista è vuota
    if(list->num_elem == 0) {
        list->HEAD = new_elem;
        list->TAIL = new_elem;
        list->num_elem++;
        return 0;
    }

    new_elem->next = list->HEAD;
    list->HEAD = new_elem;
    list->num_elem++;

    return 0;

}

elem_t* get_head(list_t* list){

    if(!list || list->num_elem == 0)
        return NULL;

    elem_t* head_elem = list->HEAD;
    list->HEAD = list->HEAD->next;
    list->num_elem--;

    return head_elem;
}

elem_t* get_tail(list_t* list){
/*TODO Non posso farlo se non è una doube linked list, cioè si posso farlo ma in O(n)*/
    if(!list || list->num_elem == 0 || (list->HEAD) == NULL)
        return NULL;

    if(list->TAIL){
        if(list->num_elem == 1){
            elem_t* return_elem = list->TAIL;
            list->num_elem = 0;
            list->HEAD = NULL;
            list->TAIL = NULL;
            return return_elem;
        }
        else{
            elem_t* return_elem = list->TAIL;
            list->TAIL = list->TAIL->previous;
            list->num_elem--;
            return return_elem;
        }


    }
    return NULL;
    /*elem_t* elem = list->HEAD;
    elem_t* last_elem = NULL;
    elem_t* penultimate_elem = NULL;
    while(elem != NULL){
        penultimate_elem = last_elem;
        last_elem = elem;
        elem = elem->next;
    }
    if(penultimate_elem)
        penultimate_elem->next = NULL;
    list->TAIL = penultimate_elem;
    list->num_elem--;

    if(last_elem)
        return last_elem;
    else
        return NULL;
        */
}

elem_t* get_elem(list_t* list, void* value, int (*compare_function)(void*, void*)){

    if(!list || !compare_function || !list->HEAD)
        return NULL;

    elem_t* elem = list->HEAD;
    elem_t* before_elem = elem;
    while(elem != NULL){
        if( compare_function(elem->content, value) == 0 ){
            printf("DEBUG, TROVATO!\n");
            //Se c'è solo un elemento
            if(list->num_elem == 1){
                list->HEAD = NULL;
                list->TAIL = NULL;
                list->num_elem = 0;
                return elem;
            }
            //Se c'è più di un elemento ed è il primo
            if(elem == list->HEAD){
                //Il nuovo HEAD ora è il secondo
                list->HEAD = elem->next;
                //Il precedente di HEAD è NULL
                list->HEAD->previous = NULL;
                list->num_elem--;
                return elem;
            }

            //Se c'è più di un elemento ed è l'ultimo
            if(elem == list->TAIL){
                //il nuovo TAIL ora è il penultimo
                list->TAIL = elem->previous;
                //Il successivo di TAIL è NULL
                list->TAIL->next = NULL;
                list->num_elem--;
                return elem;
            }

            //Se è un elemento centrale
            before_elem->next = elem->next;
            elem->next->previous = before_elem;
            list->num_elem--;
            return elem;

        }
        before_elem = elem;
        elem = elem->next;
    }
    //Se non l'ho trovato
     return NULL;
}

void list_destroy(list_t* list, void (*free_content)(void*)){

    if(list == NULL)
        return;

    while(list->num_elem != 0){
       elem_t* toDestroy = get_head(list);
       if(free_content && toDestroy->content)
           (free_content)(toDestroy->content);
       free(toDestroy);
    }

    if(list) {
        free(list);
        list = NULL;
    }

}