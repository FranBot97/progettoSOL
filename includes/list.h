//
// Created by linuxlite on 21/02/22.
//

#ifndef LIST_H
#define LIST_H
#include "file.h"
#include "stdlib.h"

typedef struct elem_{

    struct elem_* next;
    struct elem_* previous;
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

elem_t* get_head(list_t* list);

elem_t* get_tail(list_t* list);

elem_t* get_elem(list_t* list, void* value, int (*)(void*, void*));

void list_destroy(list_t* list, void (*)(void*));

#endif //LIST_H
