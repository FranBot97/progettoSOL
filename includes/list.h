//
// Created by linuxlite on 21/02/22.
//

#ifndef NOVEMBREPROG_LIST_H
#define NOVEMBREPROG_LIST_H
#include "file.h"
#include "stdlib.h"

typedef struct elem_{

    struct elem_* next;
    void* content;

}elem_t;

typedef struct list{

    elem_t* HEAD;
    elem_t* TAIL;
    int num_elem;

}list_t;

list_t* list_create();

int add_tail_element(list_t* list, void* content);

int add_head_element(list_t* list, void* content);

void* remove_head(list_t* list);

void* remove_tail(list_t* list);

void list_destroy(list_t* list);

#endif //NOVEMBREPROG_LIST_H
