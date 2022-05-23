#ifndef NOVEMBREPROG_API_H
#define NOVEMBREPROG_API_H
#include <stdbool.h>
#include <list.h>
#include <util.h>

/**
 * @brief Apre una connessione verso il server tramite la socket indicata.
 *
 * @param sockname - nome della socket a cui connettersi
 * @param msec - intervallo in ms dopo il quale si tenta la riconnessione
 * @param abstime - al raggiungimento del tempo di timeout si interrompe il tentativo di connessione
 * @return 0 se OK, -1 in caso di errore, setta errno opportunamente
 */
int openConnection(const char* sockname, int msec, const struct timespec abstime);

/**
 * @brief Chiude la connessione verso il server tramite la socket indicata
 *
 * @param sockname - nome della socket da cui disconnettersi
 * @return 0 se Ok, -1 in caso di errore, setta errno opportunamente
 */
int closeConnection(const char* sockname);

/**
 * @brief Apre il file sul server
 * @param pathname - file da aprire
 * @param flags - modalità di apertura
 * @return 0 se OK, -1 in caso di errore, setta errno opportunamente
 */
int openFile(const char* pathname, int flags);

/**
 * @brief Legge il contenuto del file dal server
 *
 * @param pathname - nome del file da leggere
 * @param buf - buffer dove memorizzare il contenuto
 * @param size - dimensione del contenuto ricevuto in bytes
 * @return 0 se OK, -1 in caso di errore, setta errno opportunamente. In caso di errore buf e size non sono validi
 */
int readFile(const char* pathname, void** buf, size_t* size);

/**
 * @brief Legge N files dal server
 *
 * @param N - Numero di file da leggere
 * @param dirname - eventuale cartella dove memorizzare i file letti
 * @return 0 se OK, -1 in caso di errore, setta errno opportunamente.
 */
int readNFiles(int N, const char* dirname);

/**
 * @brief Scrive nel server il contenuto del file e salva nella cartella dirname eventuali file espulsi
 *
 * @param pathname - nome del file da scrivere
 * @param dirname - cartella in cui salvare eventuali file espulsi
 * @return 0 se Ok, -1 in caso di errore, setta errno opportunamente
 */
int writeFile(const char* pathname, const char* dirname);

/**
 * @brief Scrive il contenuto dentro buf in aggiunta al file pathname sul server.
 * Salva nella cartella eventuali file espulsi
 *
 * @param pathname - nome del file a cui aggiungere il contenuto
 * @param buf - contenuto da aggiungere
 * @param size - dimensione del contenuto in bytes
 * @param dirname - cartella in cui salvare eventuali file espulsi
 * @return 0 se Ok, -1 in caso di errore, setta errno opportunamente
 */
int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname);

/**
 * @brief Acquisisce la mutua esclusione sul file
 *
 * @param pathname - file su cui acquisire la mutua esclusione
 * @return 0 se Ok, -1 in caso di errore, setta errno opportunamente
 */
int lockFile(const char* pathname);


/**
 * @brief Rilascia la mutua esclusione sul file
 *
 * @param pathname - file su cui rilasciare la mutua esclusione
 * @return 0 se Ok, -1 in caso di errore, setta errno opportunamente
 */
int unlockFile(const char* pathname);

/**
 * @brief Chiude il file nel server
 *
 * @param pathname - file da chiudere
 * @return 0 e Ok, -1 in caso di errore, setta errno opportunamente
 */
int closeFile(const char* pathname);

/**
 * @brief Cancella il file dal server
 * @param pathname - file da rimuovere
 * @return 0 se Ok, -1 in caso di errore
 */
int removeFile(const char* pathname);

#endif
