#ifndef READ_WRITE_LOCK_H
#define READ_WRITE_LOCK_H
#include <pthread.h>
#include <stdbool.h>
//Semplice mutex per lettori e scrittori, può essere resa più fair.

/**
 * Struttura mutex per lettori e scrittori
 *
 * @param mutex - mutex generica
 * @param condition - variabile di condizione associata alla mutex per
 * segnalare ai lettori/scrittori che è avvenuto un evento
 * @param writer - quando è true indica che è presente uno scrittore
 * @param active_readers - numero di lettori attivi
 *
 */
typedef struct rwlock{
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    bool writer;
    unsigned int active_readers; //numero di lettori
}rwlock_t;

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
 * @param lock - struttura su cui acquisire la lock, non può essere NULL
*/
int rwlock_readerUnlock(rwlock_t* lock);

/**
 * @brief Acquisisce la lock per uno scrittore
 * @returns 0, altrimenti -1 in caso di errore
 * @param lock - struttura su cui acquisire la lock, non può essere NULL
*/
int rwlock_writerLock(rwlock_t* lock);

/**
 * @brief Rilascia la lock per uno scrittore
 * @returns 0, altrimenti -1 in caso di errore
 * @param lock - struttura su cui acquisire la lock, non può essere NULL
*/
int rwlock_writerUnlock(rwlock_t* lock);

/**
 * @brief Libera le risorse
 * @param lock - struttura da deallocare
*/
void rwlock_destroy(rwlock_t* lock);

#endif //READ_WRITE_LOCK_H
