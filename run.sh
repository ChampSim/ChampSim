#!/bin/bash
# Script to run the ChampSim simulator

TRACEFILE="../400.perlbench-41B.champsimtrace.xz"
OUTFILE="out.txt"
# 20K instructions
WARMUP=20000
# 10M instructions
# Note: anything less than about 5M instructions will not cause any misses
SIMINSTR=10000000

# Clean existing if required
make clean
printf "Cleaning existing build...\n"

# First configure the simulator
printf "Configuring ChampSim..."
./config.sh champsim_config.json
printf "Done.\n"

# Make the simulator using given config
make

printf "Running ChampSim..."

# Run the simulator
./bin/champsim --warmup_instructions "$WARMUP" --simulation_instructions "$SIMINSTR" "$TRACEFILE" "$TRACEFILE" >"$OUTFILE"

printf "Done.\n"

printf "Output written to %s" "$OUTFILE"
