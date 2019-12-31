#!/bin/bash

while read LINE
do
    wget -P $PWD/dpc3_traces -c http://hpca23.cse.tamu.edu/champsim-traces/speccpu/$LINE
done < selectedTraces.txt