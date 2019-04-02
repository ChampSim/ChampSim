#!/usr/bin/env bash

BRANCH_PREDICTORS=$1
L1D_PREFETCHERS=$2
L2C_PREFETCHERS=$3
LLC_PREFETCHERS=$4
REPLACEMENT_POLICIES=$5

CMAKE_WD=$PWD

# Doing some sanity checks before modifying any states.
if [[ $# -ne 5 ]]; then
	printf "The command is provided an illegal number of arguments. %s parameter(s) given but 5 required.\n" $#
	exit 1
fi

if [[ ! -e $PWD/$BRANCH_PREDICTORS ]]; then
	printf "The file ${PWD}/${BRANCH_PREDICTORS} couldn't be found. Exiting.\n"
	exit 1
fi

if [[ ! -e $PWD/$L1D_PREFETCHERS ]]; then
	printf "The file ${PWD}/${L1D_PREFETCHERS} couldn't be found. Exiting.\n"
	exit 1
fi

if [[ ! -e $PWD/$L2C_PREFETCHERS ]]; then
	printf "The file ${PWD}/${l2C_PREFETCHERS} couldn't be found. Exiting.\n"
	exit 1
fi

if [[ ! -e $PWD/$LLC_PREFETCHERS ]]; then
	printf "The file ${PWD}/${LLC_PREFETCHERS} couldn't be found. Exiting.\n"
	exit 1
fi

if [[ ! -e $PWD/$REPLACEMENT_POLICIES ]]; then
	printf "The file ${PWD}/${REPLACEMENT_POLICIES} couldn't be found. Exiting.\n"
	exit 1
fi

# Now that we have triggered all the sanity checks on the given parameters,
# we need to make sure that the directory architecture fits our needs.
#
# These tests are non critical; if a condition fails we can revert to a safe
# state.
if [[ -d $PWD/build ]]; then
	rm -r $PWD/build/
fi

if [[ -d $PWD/bin ]]; then
	rm -r $PWD/bin/
fi

# All is set, we just need to get all the configurations parameters from
# the provided files and execute the associated commands.
branch_predictors=()
l1d_prefetchers=()
l2c_prefetchers=()
llc_prefetchers=()
replacement_policies=()

while IFS='' read -r line; do
	branch_predictors=("${branch_predictors[@]}" "$line")
done < $PWD/$BRANCH_PREDICTORS

while IFS='' read -r line; do
	l1d_prefetchers=("${l1d_prefetchers[@]}" "$line")
done < $PWD/$L1D_PREFETCHERS

while IFS='' read -r line; do
	l2c_prefetchers=("${l2c_prefetchers[@]}" "$line")
done < $PWD/$L2C_PREFETCHERS

while IFS='' read -r line; do
	llc_prefetchers=("${llc_prefetchers[@]}" "$line")
done < $PWD/$LLC_PREFETCHERS

while IFS='' read -r line; do
	replacement_policies=("${replacement_policies[@]}" "$line")
done < $PWD/$REPLACEMENT_POLICIES

for branch_predictor in "${branch_predictors[@]}"; do
	for l1d_prefetcher in "${l1d_prefetchers[@]}"; do
		for l2c_prefetcher in "${l2c_prefetchers[@]}"; do
			for llc_prefetcher in "${llc_prefetchers[@]}"; do
				for replacement_policy in "${replacement_policies[@]}"; do
					build_dir="build/$branch_predictor/$l1d_prefetcher/$l2c_prefetcher/$llc_prefetcher/$replacement_policy/"

					mkdir -p $build_dir
					pushd $build_dir

					# Running the CMake buildsystem with the appropriate knobs.
					cmake $CMAKE_WD -G "Unix Makefiles" -DCHAMPSIM_BRANCH_PREDICTOR:STRING=$branch_predictor -DCHAMPSIM_PREFETCHER_L1D:STRING=$l1d_prefetcher -DCHAMPSIM_PREFETCHER_L2C:STRING=$l2c_prefetcher -DCHAMPSIM_PREFETCHER_LLC:STRING=$llc_prefetcher -DCHAMPSIM_REPLACEMENT_POLICY:STRING=$replacement_policy
					make

					popd
				done
			done
		done
	done
done
