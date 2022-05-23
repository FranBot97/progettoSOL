#ifndef STORAGE_H
#define STORAGE_H

#include <icl_hash.h>
#include <list.h>
#include <util.h>
#include <pthread.h>
#include <read_write_lock.h>

/**
 * Struttura di un file
 * @param filename - nome del file
 * @param size - dimensione del contenuto in bytes
 * @param content - contenuto generico
 * @param client_locker - client che detiene la lock, -1 se è libero
 * @param who_opened - lista di client che hanno il file aperto
 * @param mutex - mutex per read-write lock
 */
typedef struct file_{
    char* filename;
    unsigned int size;
    void* content;
    int client_locker;
    list_t* who_opened;
    rwlock_t* mutex;
}file_t;

/**
 * Struttura dello storage
 * @param files_limit - numero massimo di file con contenuto > 0 conservabili
 * @param memory_limit - massima memoria dello storage in bytes
 * @param files_number - attuale numero di file con contenuto > 0
 * @param occupied_memory - memoria attualmente occupata in bytes
 * @param files - tabella hash contenente i file
 * @param filenames_queue - lista di nomi di file con contenuto > 0 usata per la selezione vittime
 * @param mutex - mutex per la read-write lock
 * @param clients_waiting_lock - array contenente gli id dei client che sono in attesa di prendere la lock su un file
 * @param max_files_number - massimo numero di file con contenuto > 0 raggiunto nello storage
 * @param max_occupied_memory - massima dimensione della memoria raggiunta in bytes
 * @param times_replacement_algorithm - numero di volte che è stato eseguito l'algoritmo di rimpiazzamento
 */
typedef struct storage_{

    int files_limit;
    unsigned long long memory_limit; //in bytes

    int files_number;
    unsigned long long occupied_memory; //in bytes
    icl_hash_t* files;
    list_t* filenames_queue;
    rwlock_t* mutex;
    int clients_waiting_lock[MAX_CONN];

    //Statistiche
    int max_files_number;
    unsigned long long max_occupied_memory;
    int replace_mode;
    int times_replacement_algorithm;

}storage_t;

/**
 * @brief Crea un nuovo storage con i valori passati come parametro
 *
 * @param max_file - massimo numero di file consentiti, deve essere > 0
 * @param max_memoria - massima memoria in bytes, deve essere > 0
 * @param replace_mode - modalità di rimpiazzamento
 * @return puntatore allo storage creato o NULL in caso di errore
 */
storage_t* storage_create(int max_file, unsigned long long max_memoria, int replace_mode);

/**
 * @brief Dealloca lo storage
 *
 * @param storage - puntatore allo storage da deallocare
 */
void storage_destroy(storage_t* storage);

/**
 * @brief Dealloca un file
 *
 * @param myfile - puntatore al file da deallocare
 */
void file_destroy(file_t *myfile);

//Operazioni sui file//
/**
 * @brief Apre un file sullo storage. Se non è presente il file viene creato solo se è stato passato
 * il flag O_CREATE, altrimenti se il client ha tutti i permessi viene aperto con i flag corrispondenti.
 *
 * @param storage - storage su cui eseguire l'operazione
 * @param filename - nome del file da aprire
 * @param flags - possono essere O_CREATE o O_LOCK in base all'azione che si vuole eseguire
 * @param client - id del client
 * @return 0 se OK, < 0 in caso di errore, imposta errno.
 */
int storage_openFile(storage_t* storage, char* filename, int flags, int client);

/**
 * @brief Legge un file dallo storage se esiste e se l'utente ha i permessi per accederci.
 * Memorizza il contenuto nel buffer "buf", se il file è vuoto viene restituito un errore.
 *
 * @param storage - storage su cui effettuare l'operazione
 * @param filename - nome del file da leggere
 * @param client - id del client
 * @param buf - buffer dove salvare il contenuto del file
 * @return bytes letti > 0 se OK, <= 0 in caso di errore, imposta errno
 */
long long storage_readFile(storage_t* storage, char* filename, int client, void** buf);
/**
 * @brief Legge al massimo N file casuali dallo storage con contenuto > 0, se N <= 0 vengono letti tutti.
 * Possono essere letti solo i file che non sono in stato "locked" da parte di un altro client.
 * I file vengono memorizzati nella lista "files_to_send".
 *
 * @param storage - storage su cui effettuare l'operazione
 * @param client - id del client
 * @param N - Numero di file da leggere
 * @param files_to_send - lista in cui memorizzare i file letti
 * @return numero dei file letti se Ok, < 0 in caso di errore, imposta errno
 */
int storage_readNFiles(storage_t* storage, int client, int N, list_t** files_to_send);

/**
 * @brief Effettua la prima scrittura sul file "filename".
 * Il file deve essere vuoto e in modalità "locked" da parte del client.
 * Può causare l'espulsione di altri file che vengono memorizzati nella lista "list".
 *
 * @param storage - storage su cui effettuare l'operazione
 * @param filename - nome del file da scrivere
 * @param file_size - dimensione del contenuto da scrivere
 * @param file_content - contenuto
 * @param client - id del client
 * @param list - lista in cui memorizzare eventuali file espulsi
 * @return 0 se Ok, < 0 in caso di errore, imposta errno
 */
int storage_writeFile(storage_t* storage, const char* filename, size_t file_size, void* file_content, int client, list_t** list);

/**
 * @brief Effettua una scrittura in append al file. Può causare l'espulsione di altri file che vengono
 * memorizzati nella lista "list"
 *
 * @param storage - storage su cui effettuare l'operazione
 * @param filename - nome del file da scrivere
 * @param size - dimensione del contenuto da aggiungere in bytes
 * @param data - contenuto da aggiungere al file
 * @param client - id client
 * @param list - lista in cui memorizzare i file espulsi
 * @return 0 se Ok, < 0 in caso di errore, setta errno opportunamente
 */
int storage_appendToFile(storage_t* storage, char* filename, size_t size, void* data, int client, list_t** list);

/**
 * @brief Tenta di prendere la mutua esclusione sul file "filename", se la lock sul file
 * è al momento detenuta da un altro client la richiesta fallisce.
 *
 * @param storage - storage su cui effettuare l'operazione
 * @param filename - nome del file
 * @param client - id del client
 * @return 0 se Ok, < 0 in caso di errore, imposta errno
 */
int storage_lockFile(storage_t* storage, char* filename, int client);

/**
 * @brief Rilascia la mutua esclusione sul file "filename".
 *
 * @param storage - storage su cui effettuare l'operazione
 * @param filename - nome del file
 * @param client - id del client
 * @return 0 se Ok, < 0 in caso di errore, imposta errno
 */
int storage_unlockFile(storage_t* storage, const char* filename, int client);

/**
 * @brief Chiude il file da parte del client che esegue la richiesta
 *
 * @param storage - storage su cui eseguire la richiesta
 * @param filename - nome del file da chiudere
 * @param client - id del client
 * @return 0 se Ok, < 0 in caso di errore, imposta errno
 */
int storage_closeFile(storage_t* storage, char* filename, int client);

/**
 * @brief Rimuove il file dallo storage verificando che l'utente abbia i permessi necessari.
 * Il numero dei bytes rimossi viene salvato in "deleted_bytes"
 *
 * @param storage - storage su cui eseguire l'operazione
 * @param filename - nome del file da cancellare
 * @param client - id del client
 * @param deleted_bytes - bytes rimossi
 * @return 0 se Ok, < 0 in caso di errore
 */
int storage_removeFile(storage_t* storage, char* filename, int client, unsigned int* deleted_bytes);

// Funzioni di supporto //

/**
 * @brief Aggiunge l'id del client all'array dello storage contenente gli id
 * dei client che stanno aspettando di effettuare un lock con successo
 *
 * @param storage - storage su cui effettuare l'operazione
 * @param client - id del client da aggiungere
 * @return 0 se ok, < 0 in caso di errore, imposta errno
 */
int storage_addClientWaiting(storage_t* storage, int client);

/**
 * @brief Preleva un client dall'array dei client in attesa
 *
 * @param storage - storage su cui effettuare l'operazione
 * @return id del client, < 0 in caso di errore
 */
int storage_removeClientWaiting(storage_t* storage);

/**
 * @brief Stampa le statistiche dello storage
 * @param storage
 * @return 0 se Ok, < 0 in caso di errore, setta errno
 */
int storage_printStats(storage_t *storage);

#endif //STORAGE_H
