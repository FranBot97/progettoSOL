//
// Created by linuxlite on 21/02/22.
//
#include "../includes/list.h"
#include <stdlib.h>


list_t* list_create() {

    list_t *return_list = (list_t *) malloc(sizeof(list_t));
    return_list->HEAD = NULL;
    return_list->TAIL = NULL;
    return_list->num_elem = 0;

    return return_list;
}

int add_tail_element(list_t* list, void* content){

    if(!list)
        return -1;

    elem_t* new_elem;
    if( (new_elem = (elem_t*)malloc(sizeof(elem_t))) == NULL)
        return -1;
    new_elem->content = content;
    new_elem->next = NULL;

    //Se la lista è vuota
    if(list->num_elem == 0) {
        list->HEAD = new_elem;
        list->TAIL = new_elem;
        list->num_elem++;
        return 0;
    }

    list->TAIL->next = new_elem;
    list->TAIL = new_elem;
    list->num_elem++;

    return 0;

}

int add_head_element(list_t* list, void* content){

    if(!list)
        return -1;

    elem_t* new_elem;
    if( (new_elem = (elem_t*)malloc(sizeof(elem_t))) == NULL)
        return -1;
    new_elem->content = content;
    new_elem->next = NULL;

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

void* remove_head(list_t* list){

    if(!list || list->num_elem == 0)
        return NULL;

    elem_t* head_elem = list->HEAD;
    list->HEAD = list->HEAD->next;
    list->num_elem--;
    void* return_content = head_elem->content;
    free(head_elem);

    return return_content;
}

void* remove_tail(list_t* list){
/*TODO Non posso farlo se non è una doube linked list, cioè si posso farlo ma in O(n)*/
    if(!list || list->num_elem == 0 || (list->HEAD) == NULL)
        return NULL;

    elem_t* elem = list->HEAD;
    elem_t* last = NULL;
    while(elem != NULL){
        last = elem;
        elem = elem->next;
    }
    if(last)
        last->next = NULL;
    list->TAIL = last;
    list->num_elem--;

    return elem->content;
}

void list_destroy(list_t* list){

    if(list == NULL)
        return;

    while(list->num_elem != 0){
        remove_head(list);
    }

    if(list) {
        free(list);
        list = NULL;
    }

}