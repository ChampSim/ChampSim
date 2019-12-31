#!/bin/bash

echo "Running trace applications on baseline processors"
echo " "

BIN="bimodal-no-no-no-no-lru-1core"
echo "Running bin ->" $BIN
while read LINE
do
echo "Running trace -> " $LINE
./run_champsim.sh $BIN 50 200 $LINE
done < selectedTraces.txt
echo " "
echo "Done "$BIN
echo " "

BIN="bimodal-next_line-next_line-ip_stride-no-lru-1core"
echo "Running bin -> "$BIN
while read LINE
do
echo "Running trace -> " $LINE
./run_champsim.sh $BIN 50 200 $LINE
done < selectedTraces.txt
echo " "
echo "Done "$BIN
echo " "

BIN="hashed_perceptron-next_line-next_line-kpcp-next_line-drrip-1core" 
echo "Running bin -> "$BIN
while read LINE
do
echo "Running trace -> " $LINE
./run_champsim.sh $BIN 50 200 $LINE
done < selectedTraces.txt
echo " "
echo "Done "$BIN
echo " "
echo " "
echo "DONE ALL"