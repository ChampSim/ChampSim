
#Running SERVER simulations for 50-50M instructions
numwarmup=50
numsim=50

parallelismcount=16

tracedir=./DPC-traces/
traces=$(cat scripts/spec2017_trace_list.txt)
results_folder=spec2017_ptw_results/

make clean
make


tracenum=0
count=0
iterator=0

for((i=0; i<$numofpref; i++))
do 
		for trace in $traces;
		do
			if [ ${count} -lt ${parallelismcount} ]
            then
				bin/champsim -warmup_instructions ${numwarmup}000000 -simulation_instructions ${numsim}000000 -traces ${tracedir}${trace} &> ${results_folder}${trace}.txt &
			count=`expr $count + 1`

			else
				bin/champsim -warmup_instructions ${numwarmup}000000 -simulation_instructions ${numsim}000000 -traces ${tracedir}${trace} &> ${results_folder}${trace}.txt
				count=0
			fi
		done
		now=$(date)
		echo "Number of SERVER traces simulated - $count in iteration $iterator at time: $now" 

done

echo "Done with count $count"
