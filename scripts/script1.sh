#!/bin/bash
echo -e "========TEST 1 STARTING========="
echo -e "Starting server..."
valgrind --leak-check=full ./server -f ./test/test1/config1.txt &
# server pid
SERVER_PID=$!
export SERVER_PID

sleep 5s

STORE_DIR=./test/test1/store_files
UNLOCKED=img1.jpg

echo -e "Starting clients..."
./client -h
./client -p -t 200 -f socket1.sk
# Scritture e letture dallo stesso client
./client -p -t 200 -f socket1.sk -w ./test/test1/send_files -D "$STORE_DIR" -u "$UNLOCKED" -W ./test/test1/send_files/text1.txt -r text1.txt -d "$STORE_DIR"
# Lock e rimozione di file
./client -p -t 200 -f socket1.sk -l "$UNLOCKED" -c "$UNLOCKED"
# Lettura di N files
./client -p -t 200 -f socket1.sk -R -d "$STORE_DIR"
# Scrittura di file
./client -p -t 200 -f socket1.sk -W ./test/test1/send_files/images/img1.jpg,./test/test1/send_files/images/img2.jpg -D "$STORE_DIR"
# Lettura di file salvandoli nella cartella specificata
./client -p -t 200 -f socket1.sk -r img1.jpg,img2.jpg -d "$STORE_DIR"
# Lettura di 2 file dal server salvandoli nella cartella specificata
./client -p -t 200 -f socket1.sk -R 2 -d "$STORE_DIR"


echo ""
echo -e "Terminating server with SIGHUP"
kill -s SIGHUP $SERVER_PID
wait $SERVER_PID
echo ""
echo -e "========TEST 1 COMPLETED==========="
