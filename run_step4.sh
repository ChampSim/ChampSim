#!/bin/bash

TRACES=(
    "403.gcc-16B.champsimtrace.xz"
    "605.mcf_s-1554B.champsimtrace.xz"
    "619.lbm_s-2676B.champsimtrace.xz"
)

PREDICTORS=(
    "twobit_1k"
    "twobit_2k"
    "twobit_4k"
    "twobit_8k"
    "twobit_16k"
)

for PRED in "${PREDICTORS[@]}"
do
    echo "======================================"
    echo "Building $PRED"
    echo "======================================"

    ./build_champsim.sh "$PRED" no no no no lru 1

    BINARY="${PRED}-no-no-no-no-lru-1core"

    for TRACE in "${TRACES[@]}"
    do
        echo "======================================"
        echo "Predictor: $PRED"
        echo "Trace:     $TRACE"
        echo "======================================"

        ./run_champsim.sh \
            "$BINARY" \
            1 \
            10 \
            "$TRACE"
    done
done
