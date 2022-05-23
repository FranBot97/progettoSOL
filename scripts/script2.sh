#!/bin/bash
echo -e "========TEST 2 STARTING========="
echo -e "Starting server..."
./server -f ./test/test2/config2.txt &
# server pid
SERVER_PID=$!
export SERVER_PID

sleep 5s

STORE_VICTIMS1=./test/test2/victims1
STORE_VICTIMS2=./test/test2/victims2
FIRST_FILES=./test/test2/send_files/text/send_first
SECOND_FILES=./test/test2/send_files/text/send_later

echo -e "Starting clients..."
# Invio i primi 4 file (text1 - text4)
./client -p -f socket2.sk -w "$FIRST_FILES" -D "$STORE_VICTIMS1"
# Il secondo client invia i file da text5 a text14
# ci si aspetta di trovare nella cartella "victims1" i file text1-text4
./client -p -f socket2.sk -w "$SECOND_FILES" -D "$STORE_VICTIMS1"
# Il terzo client invia le 3 immagini da 3KB ciascuna.
# ci si aspetta di trovare nella cartella "victims2" alcuni dei file precedenti
# in base all'ordine in cui sono stati processati nella cartella
./client -p -f socket2.sk -w ./test/test2/send_files/images -D "$STORE_VICTIMS2"

echo ""
echo -e "Terminating server with SIGHUP"
kill -s SIGHUP $SERVER_PID
wait $SERVER_PID
echo -e "========TEST 2 COMPLETED==========="