#!/bin/bash

if [ $# -eq 0 ]; then
	echo "Nessun file di log specificato, caricamente del file di log di default.."
	LOG_FILE=../logs/logs.txt
	else
	    LOG_FILE=$1
fi

# Se il file è vuoto esco
[ -s "$LOG_FILE" ] && echo "Inizio il parsing del file di log.." || exit 1

#  ----READ----
echo "=======[ READ OPERATION ]========"
declare -i sum=0;
#Conto le righe che contengono l'operazione "READ", vengono considerate anche le READ_N
number_of_total_reads=$(grep -c "/OP/=READ" "$LOG_FILE")
echo "READ totali:" "$number_of_total_reads"
#Conto soltanto le righe che sono andate a buon fine tra quelle con l'operazione READ
number_of_ok_reads=$(grep "/OP/=READ" "$LOG_FILE" | grep -c "/OUTCOME/=OK")
echo "READ andate a buon fine:" "$number_of_ok_reads"
#Faccio la somma dei valori di "SENT_BYTES" per ogni READ andata a buon fine
sum=$(grep "/OP/=READ" "$LOG_FILE" | grep "/OUTCOME/=OK" | cut -d ' ' -f6 | cut -d '=' -f2 |  awk '{ SUM += $1} END { print SUM }')
echo "Bytes totali letti:" "$sum"
# calcolo media
if [  "${number_of_ok_reads}" -ne 0 ]; then
read_avg=$(echo "scale=2; ${sum} / ${number_of_ok_reads}" | bc -l)
echo "Bytes letti in media" "$read_avg"
else
echo "Bytes letti in media: 0"
fi
echo "=================================="

# ----WRITE + WRITE_APPEND----
echo "=======[ WRITE OPERATION ]========"
declare -i sum=0;
number_of_total_writes=$(grep -c "/OP/=WRITE" "$LOG_FILE")
echo "WRITE totali:" "$number_of_total_writes"
number_of_ok_writes=$(grep "/OP/=WRITE" "$LOG_FILE" | grep -c "/OUTCOME/=OK")
echo "WRITE andate a buon fine:"  "$number_of_ok_writes"
sum=$(grep "/OP/=WRITE" "$LOG_FILE" | grep "/OUTCOME/=OK" | cut -d ' ' -f5 | cut -d '=' -f2 |  awk '{ SUM += $1} END { print SUM }')
echo "Bytes totali scritti:" "$sum"
if [ "${number_of_ok_writes}" -ne 0 ]; then
write_avg=$(echo "scale=2; ${sum} / ${number_of_ok_writes}" | bc -l)
echo "Bytes scritti in media" "$write_avg"
else
  echo "Bytes scritti in media: 0"
fi
echo "=================================="

# ---LOCK----
echo "=======[ LOCK OPERATION ]========"
number_of_trylock=$(grep -c "/OP/=LOCK" "$LOG_FILE" )
number_of_ok_lock=$(grep "/OP/=LOCK" "$LOG_FILE" | grep -c "/OUTCOME/=OK")
number_of_openlock1=$(grep -c "/OP/=OPEN_LOCK" "$LOG_FILE")
number_of_ok_openlock1=$(grep "/OP/=OPEN_LOCK" "$LOG_FILE" | grep -c "/OUTCOME/=OK")
number_of_openlock2=$(grep -c "/OP/=OPEN_CREATE_LOCK" "$LOG_FILE")
number_of_ok_openlock2=$(grep "/OP/=OPEN_CREATE_LOCK" "$LOG_FILE" | grep -c "/OUTCOME/=OK")
echo "Tentativi di LOCK totali:" "$number_of_trylock"
echo "Tentativi di LOCK andati a buon fine:" "$number_of_ok_lock"
echo "OPENLOCK totali:" $((number_of_openlock1+number_of_openlock2))
echo "OPENLOCK andate a buon fine:" $((number_of_ok_openlock1+number_of_ok_openlock2))
echo "LOCK e OPENLOCK andate a buon fine:" $((number_of_ok_openlock1+number_of_ok_openlock2+number_of_ok_lock))
echo "=================================="

# --- UNLOCK ----
echo "=======[ UNLOCK OPERATION ]========"
number_of_unlock=$(grep -c "/OP/=UNLOCK" "$LOG_FILE")
number_of_ok_unlock=$(grep "/OP/=UNLOCK" "$LOG_FILE" | grep -c "/OUTCOME/=OK")
echo "UNLOCK totali:" "$number_of_unlock"
echo "UNLOCK andate a buon fine:" "$number_of_ok_unlock"
echo "=================================="

# ---- OPEN ----
echo "=======[ OPEN OPERATION ]========"
number_of_total_open=$(grep -c "/OP/=OPEN" "$LOG_FILE")
number_of_ok_open=$(grep "/OP/=OPEN" "$LOG_FILE" | grep -c "/OUTCOME/=OK")
echo "OPEN totali:" "$number_of_total_open"
echo "OPEN andate a buon fine:" "$number_of_ok_open"
echo "=================================="

# ----- CLOSE ----
echo "=======[ CLOSE OPERATION ]========"
number_of_total_close=$(grep -c "/OP/=CLOSE" "$LOG_FILE")
number_of_ok_close=$(grep "/OP/=CLOSE" "$LOG_FILE" | grep -c "/OUTCOME/=OK")
echo "CLOSE totali:" "$number_of_total_close"
echo "CLOSE andate a buon fine:" "$number_of_ok_close"
echo "=================================="

# ---- ALGORITMO DI RIMPIAZZAMENTO ----
echo "=======[ REPLACEMENT ALGORITHM ]=========================="
number_of_replacements=$(grep "/OP/=VICTIM" "$LOG_FILE" | grep -c "/OUTCOME/=OK")
echo "Numero di esecuzioni dell'algoritmo di rimpiazzamento:" "$number_of_replacements"
echo "=========================================================="


# ---- INFO ------
echo "===================[ OTHER INFO ]======================"

# --- CONNESSIONI ------
# Calcolo l'ID minimo e l'ID massimo tra tutti i client, la differenza+1 ci dà
# il numero massimo di utenti connessi contemporaneamente
min_client=$( cut -d ' ' -f3 < "$LOG_FILE" | cut -d '=' -f2 | sort -g | head -1)
max_client=$( cut -d ' ' -f3 < "$LOG_FILE" | cut -d '=' -f2 | sort -g | tail -1)
max_connection=$((max_client-min_client+1))
echo "Massimo numero di utenti connessi contemporaneamente: "$max_connection

# ----- MB size ------
declare -i bytes_balance=0;
declare -i max_bytes=0;
# Per ogni linea del file di log se è un'operazione di WRITE effettuata
# con successo allora aggiungo i bytes relativi all'operazione.
# Se è un rimpiazzamento o una rimozione di file andati a buon fine
# allora rimuovo i bytes.
while IFS="" read -r p || [ -n "$p" ]
do
  op=$(echo "$p" | cut -d ' ' -f2 | cut -d '=' -f2)
  outcome=$(echo "$p" | cut -d ' ' -f8 | cut -d '=' -f2)
  # Aggiungo bytes
  if [[  ("${op}" == "WRITE"  || "${op}" == "WRITE_APPEND") && "${outcome}" == "OK" ]]; then
    add_bytes=$(echo "$p" | cut -d ' ' -f5 | cut -d '=' -f2 | sort -g)
    bytes_balance=$((bytes_balance + add_bytes))
    # Quando trovo un valore maggiore del massimo lo aggiorno
      if [ "${bytes_balance}" -gt "${max_bytes}" ]; then
        max_bytes=$bytes_balance
        fi
    fi
  # Rimuovo bytes
   if [[ ("${op}" == "VICTIM" || "${op}" == "REMOVE") && "${outcome}" == "OK" ]]; then
      del_bytes=$(echo "$p" | cut -d ' ' -f4 | cut -d '=' -f2)
      bytes_balance=$((bytes_balance-del_bytes))
      fi
done <<< "$(grep '/OP/=WRITE\|/OP/=VICTIM\|/OP/=REMOVE' "$LOG_FILE")"
# Conversione in MB in base 2
max_KB=$(echo "scale=4; ${max_bytes} / 1024" | bc -l)
max_MB=$(echo "scale=4; ${max_KB} / 1024" | bc -l)
echo "Memoria massima occupata nello storage in MB: " "$max_MB"

# ---- FILES -----
# Non conto i file creati con le OPEN dato che hanno peso 0
# Per ogni WRITE andata a buon fine +1
# Per ogni VICTIM o REMOVE andate a buon fine -1
declare -i file_balance=0;
declare -i max_file=0;

while IFS="" read -r p || [ -n "$p" ]
do
  op=$(echo "$p" | cut -d ' ' -f2 | cut -d '=' -f2)
  outcome=$(echo "$p" | cut -d ' ' -f8 | cut -d '=' -f2)
  # Aggiungo file
  if [[ "${op}" == "WRITE" && "${outcome}" == "OK" ]]; then
    file_balance=$((file_balance+1))
    # Quando trovo un valore maggiore del massimo lo aggiorno
      if [ ${file_balance} -gt ${max_file} ]; then
        max_file=$file_balance
      fi
  fi
  # Rimuovo file
  if [[ (${op} == "VICTIM" || ${op} == "REMOVE" ) &&  ${outcome} == "OK" ]]; then
    file_balance=$((file_balance-1))
  fi
done <<< "$(grep '/OP/=WRITE\|/OP/=VICTIM\|/OP/=REMOVE' "$LOG_FILE")"
echo "Massimo numero di file raggiunto nel server: " "$max_file"

# ---- THREADS -----
echo "---------------- WORKER THREADS --------------------"
tid_array=();
declare -i thread_counter=0;
declare -i total_requests=0;
tid_array[thread_counter]=1;
old_line=-1;
# Prendo l'elenco di tutti i worker thread ID,
# (escludendo le operazioni di connessione gestite dal main thread)
# li metto in ordine crescente, e itero sulle righe trovate.
# Conto i tasks finché il thread ID non cambia,
# ogni volta che trovo un nuovo thread ID stampo i valori del thread precedente
# e passo al thread successivo.
while read -r line ; do
    if [ "${line}" == "${old_line}" ]; then
        tid_array[thread_counter]=$((tid_array[thread_counter]+1))
    else
        if [ "${old_line}" -ne -1 ]; then
            echo "Il thread worker" "$old_line" "ha eseguito " ${tid_array[thread_counter]} " richieste."
            total_requests=$((total_requests+tid_array[thread_counter]))
            thread_counter=$((thread_counter+1));
            tid_array[thread_counter]=1;
        fi
    fi
    old_line=$line;
done <<< "$(grep -v "/OP/=CONNECT" "$LOG_FILE" | grep -v "/OP/=DISCONNECT" | cut -d ' ' -f1 | cut -d '=' -f2 | sort -g )"
 if [ "${old_line}" -ne -1 ]; then
 echo "Il thread worker" "$old_line" "ha eseguito " ${tid_array[thread_counter]} " richieste."
 fi
 echo "-----------------------------------------------------"

 total_requests=$((total_requests+tid_array[thread_counter]))
 echo "Le richieste totali sono state: " "$total_requests"
echo "======================================================="
