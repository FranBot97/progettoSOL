//
// Created by linuxlite on 12/05/22.
//
//
#ifndef PROGETTOMAGGIO_READ_WRITE_LOCK_H
#define PROGETTOMAGGIO_READ_WRITE_LOCK_H

typedef struct rwlock rwlock_t;

/**
 * @brief Inizializza la lock
 * @returns puntatore alla struttura, altrimenti NULL
*/
rwlock_t* rwlock_init();

/**
 * @brief Acquisisce la lock per un lettore
 * @returns 0, altrimenti -1 in caso di errore
 * @param lock struttura su cui acquisire la lock, non può essere NULL
*/
int rwlock_readerLock(rwlock_t* lock);

/**
 * @brief Rilascia la lock per un lettore
 * @returns 0, altrimenti -1 in caso di errore
 * @param lock struttura su cui acquisire la lock, non può essere NULL
*/
int rwlock_readerUnlock(rwlock_t* lock);

/**
 * @brief Acquisisce la lock per uno scrittore
 * @returns 0, altrimenti -1 in caso di errore
 * @param lock struttura su cui acquisire la lock, non può essere NULL
*/
int rwlock_writerLock(rwlock_t* lock);

/**
 * @brief Rilascia la lock per uno scrittore
 * @returns 0, altrimenti -1 in caso di errore
 * @param lock struttura su cui acquisire la lock, non può essere NULL
*/
int rwlock_writerUnlock(rwlock_t* lock);

/**
 * Libera le risorse
*/
void rwlock_destroy(rwlock_t* lock);

#endif //PROGETTOMAGGIO_READ_WRITE_LOCK_H
