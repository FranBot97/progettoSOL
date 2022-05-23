#include <read_write_lock.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>

rwlock_t* rwlock_init()
{
    rwlock_t* lock = NULL;
    pthread_mutex_t mutex;
    pthread_cond_t condition;

    if (pthread_mutex_init(&mutex, NULL) != 0)
        return NULL;

    if (pthread_cond_init(&condition, NULL) != 0){
        pthread_mutex_destroy(&mutex);
        return NULL;
    }

    lock = (rwlock_t*)malloc(sizeof(rwlock_t));
    if(lock == NULL){
        pthread_mutex_destroy(&mutex);
        pthread_cond_destroy(&condition);
        return NULL;
    }

    //Inizializzo
    lock->mutex = mutex;
    lock->condition = condition;
    lock->active_readers = 0;
    lock->writer = false;

    return lock;
}

int rwlock_readerLock(rwlock_t* lock)
{
    if (!lock){
        errno = EINVAL;
        return -1;
    }
    if (pthread_mutex_lock(&(lock->mutex)) != 0)
        return -1;
    //se sono un lettore controllo prima devo dare la priorità a uno scrittore
    while (lock->writer)
        pthread_cond_wait(&(lock->condition), &(lock->mutex));
    //Lettore sbloccato
    lock->active_readers++;
    if (pthread_mutex_unlock(&(lock->mutex)) != 0)
        return -1;

    return 0;
}

int rwlock_readerUnlock(rwlock_t* lock)
{
    if (!lock){
        errno = EINVAL;
        return -1;
    }
    if(pthread_mutex_lock(&(lock->mutex)) != 0)
        return -1;
    lock->active_readers--;
    if (lock->active_readers == 0) //Se non ci sono più lettori allora posso risvegliare gli scrittori
        pthread_cond_broadcast(&(lock->condition)); //In broadcast tutti si risvegliano, solo uno prende il controllo
   if(pthread_mutex_unlock(&(lock->mutex)) != 0)
        return -1;

    return 0;
}

int rwlock_writerLock(rwlock_t* lock)
{
    if (!lock){
        errno = EINVAL;
        return -1;
    }
    if (pthread_mutex_lock(&(lock->mutex)) != 0)
        return -1;
    //Se ci sono altri scrittori aspetto
    while (lock->writer)
        pthread_cond_wait(&(lock->condition), &(lock->mutex));
    //Segnalo che lo scrittore è arrivato
    lock->writer = true;
    while (lock->active_readers > 0) //Quando tutti i lettori hanno finito allora posso sbloccarmi
        pthread_cond_wait(&(lock->condition), &(lock->mutex));
    if(pthread_mutex_unlock(&(lock->mutex)) != 0)
        return -1;

    return 0;
}

int rwlock_writerUnlock(rwlock_t* lock)
{
    if (!lock){
        errno = EINVAL;
        return -1;
    }
    if (pthread_mutex_lock(&(lock->mutex)) != 0)
        return -1;
    lock->writer = false;
    pthread_cond_broadcast(&(lock->condition));
    if(pthread_mutex_unlock(&(lock->mutex)) != 0)
        return -1;

    return 0;
}

void rwlock_destroy(rwlock_t* lock){
    if (!lock)
        return;
    pthread_mutex_destroy(&(lock->mutex));
    pthread_cond_destroy(&(lock->condition));
    free(lock);
    lock = NULL;
}

