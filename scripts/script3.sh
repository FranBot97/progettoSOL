#!/bin/bash
echo -e "========TEST 2 STARTING========="

echo -e "SERVER STARTING.."
./server -f ./test/test3/config3.txt &
# server pid
SERVER_PID=$!
export SERVER_PID

sleep 5s

bash -c 'sleep 30 && kill -2 ${SERVER_PID}' &
echo -e "30 secondi all'invio del segnale SIGINT"

# faccio partire gli script per generare i client
array_id=()
for i in {1..10}; do
	bash -c './scripts/startClient.sh' &
	array_id+=($!)
	sleep 0.1
done
# Aspetto 30 secondi
sleep 30s
# stop processi
for i in "${array_id[@]}"; do
	kill "${i}"
	wait "${i}" 2>/dev/null
done

wait $SERVER_PID
killall -q ./client #chiudo eventuali processi rimasti
echo -e "========TEST 3 COMPLETED==========="