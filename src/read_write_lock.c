//
// Created by linuxlite on 12/05/22.
//

#include "../includes/read_write_lock.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>

struct rwlock
{
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    bool writer_is_waiting;
    unsigned int active_readers; //numero di lettori
};

rwlock_t* rwlock_init()
{
    int err;
    rwlock_t* lock = NULL;
    pthread_mutex_t mutex;
    pthread_cond_t condition;

    err = pthread_mutex_init(&mutex, NULL);
    if(err != 0)
        return NULL;

    err = pthread_cond_init(&condition, NULL);
    if(err != 0){
        pthread_mutex_destroy(&mutex);
        return NULL;
    }

    lock = (rwlock_t*) malloc(sizeof(rwlock_t));
    if(lock == NULL){
        pthread_mutex_destroy(&mutex);
        pthread_cond_destroy(&condition);
        return NULL;
    }

    lock->mutex = mutex;
    lock->condition = condition;
    lock->active_readers = 0;
    lock->writer_is_waiting = false;

    return lock;
}

int rwlock_readerLock(rwlock_t* lock)
{
    if (!lock)
    {
        errno = EINVAL;
        return -1;
    }
    int err;
    err = pthread_mutex_lock(&(lock->mutex));
    if (err != 0)
        return -1;

    //se sono un lettore controllo prima se devo dare la priorità a uno scrittore in attesa
    while (lock->writer_is_waiting)
        pthread_cond_wait(&(lock->condition), &(lock->mutex));
    //Lettore sbloccato
    lock->active_readers++;
    err = pthread_mutex_unlock(&(lock->mutex));
    if (err != 0)
        return -1;

    return 0;
}

int rwlock_readerUnlock(rwlock_t* lock)
{
    if (!lock)
    {
        errno = EINVAL;
        return -1;
    }
    int err;
    err = pthread_mutex_lock(&(lock->mutex));
    if (err != 0)
        return -1;
    lock->active_readers--;
    if (lock->active_readers == 0) //Se non ci sono più lettori allora posso risvegliare gli scrittori
        pthread_cond_broadcast(&(lock->condition)); //In broadcast tutti si risvegliano, solo uno prende il controllo
    err = pthread_mutex_unlock(&(lock->mutex));
    if (err != 0)
        return -1;

    return 0;
}

int rwlock_writerLock(rwlock_t* lock)
{
    if (!lock)
    {
        errno = EINVAL;
        return -1;
    }
    int err;
    err = pthread_mutex_lock(&(lock->mutex));
    if (err != 0)
        return -1;
    //Se ci sono altri scrittori aspetto
    while (lock->writer_is_waiting)
        pthread_cond_wait(&(lock->condition), &(lock->mutex));
    //Segnalo che sto aspettando
    lock->writer_is_waiting = true;
    while (lock->active_readers > 0) //Quando tutti i lettori hanno finito allora posso sbloccarmi
        pthread_cond_wait(&(lock->condition), &(lock->mutex));
    err = pthread_mutex_unlock(&(lock->mutex));
    if (err != 0)
        return -1;

    return 0;
}

int rwlock_writerUnlock(rwlock_t* lock)
{
    if (!lock)
    {
        errno = EINVAL;
        return -1;
    }
    int err;
    err = pthread_mutex_lock(&(lock->mutex));
    if (err != 0)
        return -1;
    lock->writer_is_waiting = false;
    pthread_cond_broadcast(&(lock->condition));
    err = pthread_mutex_unlock(&(lock->mutex));
    if (err != 0)
        return -1;

    return 0;
}

void rwlock_destroy(rwlock_t* lock)
{
    if (!lock) return;
    pthread_mutex_destroy(&(lock->mutex));
    pthread_cond_destroy(&(lock->condition));
    free(lock);
    lock = NULL;
}

