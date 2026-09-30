#!/usr/bin/env bash
# Run one single-core ChampSim trace.
# Usage: ./run_champsim.sh BINARY N_WARM_M N_SIM_M TRACE [OPTION]

if [ "$#" -lt 4 ] || [ "$#" -gt 5 ]; then
    echo "Illegal number of parameters" >&2
    echo "Usage: ./run_champsim.sh [BINARY] [N_WARM(M)] [N_SIM(M)] [TRACE] [OPTION]" >&2
    exit 1
fi

ROOT_DIR=$(cd -- "$(dirname -- "$0")" && pwd)
BINARY=$1
N_WARM_M=$2
N_SIM_M=$3
TRACE=$4
OPTION=${5:-}

TRACE_DIR="$ROOT_DIR/dpc3_traces"
BIN_PATH="$ROOT_DIR/bin/$BINARY"

if [ ! -f "$BIN_PATH" ]; then
    echo "[ERROR] Cannot find a ChampSim binary: $BIN_PATH" >&2
    echo "        Run ./build_champsim.sh to create it." >&2
    exit 1
fi

if [ ! -f "$TRACE_DIR/$TRACE" ]; then
    echo "[ERROR] Cannot find a trace file: $TRACE_DIR/$TRACE" >&2
    exit 1
fi

re='^[0-9]+$'
if ! [[ $N_WARM_M =~ $re ]]; then
    echo "[ERROR] N_WARM_M must be a non-negative integer (millions)." >&2
    exit 1
fi
if ! [[ $N_SIM_M =~ $re ]]; then
    echo "[ERROR] N_SIM_M must be a non-negative integer (millions)." >&2
    exit 1
fi

WARM_INST=$((N_WARM_M * 1000000))
SIM_INST=$((N_SIM_M * 1000000))
OUT_DIR="$ROOT_DIR/results_${N_SIM_M}M"
mkdir -p "$OUT_DIR"
OUT_FILE="$OUT_DIR/${TRACE}-${BINARY}${OPTION}.txt"

echo "Running $BINARY on $TRACE"
echo "Warmup: $WARM_INST instructions; simulation: $SIM_INST instructions"
echo "Output: $OUT_FILE"

# OPTION is retained for compatibility with the original wrapper, where it is
# supplied as one optional string of additional ChampSim command-line flags.
if [ -n "$OPTION" ]; then
    read -r -a OPTION_ARGS <<< "$OPTION"
else
    OPTION_ARGS=()
fi

"$BIN_PATH" \
    -warmup_instructions "$WARM_INST" \
    -simulation_instructions "$SIM_INST" \
    "${OPTION_ARGS[@]}" \
    -traces "$TRACE_DIR/$TRACE" \
    >"$OUT_FILE" 2>&1

echo "ChampSim completed successfully. Results: $OUT_FILE"
