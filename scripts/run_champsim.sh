#TRACE_DIR=/home/shpugsle/champsim_github/ChampSim/sample_traces
TRACE_DIR=/nfs/mal/static/traces/ChampSim
binary=${1}
n_warm=${2}
n_sim=${3}
trace=${4}
option=${5}

mkdir -p results_${n_sim}M
(./bin/${binary} -warmup_instructions ${n_warm}000000 -simulation_instructions ${n_sim}000000 ${option} -traces ${TRACE_DIR}/${trace}.trace.gz) &> results_${n_sim}M/${trace}-${binary}${option}.txt
