#!/bin/bash
# demo.sh — демонстрация: сервер + 5 параллельных клиентов, снимок pstree -p.
cd "$(dirname "$0")"
./server -d 1500 &
SRV=$!
sleep 0.3
./client "2+3*4" "(1+2)*(3+4)"   & C1=$!
./client "10/4" "-(5-8)*2"       & C2=$!
./client "1/0"                   & C3=$!
./client "2*(3+"                 & C4=$!
./client "3.5*2 - 1"             & C5=$!
sleep 0.5
echo "===== pstree -p $SRV (во время работы) ====="
pstree -p $SRV
echo "===== ps (во время работы) ====="
ps -o pid,ppid,stat,cmd --ppid $SRV -p $SRV
for c in $C1 $C2 $C3 $C4 $C5; do
    wait $c; echo "клиент PID=$c завершился с кодом $?"
done
kill -TERM $SRV
wait $SRV; echo "сервер PID=$SRV завершился с кодом $?"
echo "===== зомби после работы (должно быть пусто) ====="
ps -eo pid,ppid,stat,cmd | awk '$3 ~ /Z/ && /server|client/'
echo "(конец списка)"
