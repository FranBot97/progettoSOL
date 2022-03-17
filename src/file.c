//
// Created by francesco on 19/11/21.
//
#include <file.h>
#include <string.h>
#include <stdlib.h>

file_t* file_create(char* nome_file, unsigned long dimensione_file,  void* contenuto_file, int client_lock){

    file_t *testfile;
    testfile = (file_t*)malloc(sizeof(file_t));
    if (!testfile) return NULL;

    strcpy(testfile->nome_file, nome_file);
    testfile->dimensione_file = dimensione_file;
    testfile->client_lock = client_lock;
    if(contenuto_file != NULL) {
        testfile->contenuto_file = malloc(dimensione_file);
        memcpy(testfile->contenuto_file, contenuto_file, dimensione_file);
    }else{
        testfile->contenuto_file = NULL;
    }

    return testfile;
}


void file_destroy(file_t* myfile){

    if(myfile) free(myfile);
    myfile = NULL;

}

void file_clean(file_t *myfile){

    if(!myfile) return;

    //if(&(myfile->contenuto_file)) free(&(myfile->contenuto_file));
    myfile->dimensione_file = 0;
    myfile->client_lock = -1;



    file_destroy(myfile);

}
