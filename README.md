# Runtime-Context-Based Set Dueling for Cache Replacement

CS683 individual project at IIT Bombay.
This repository uses ChampSim to study LLC replacement policies and whether interval-level LLC demand-miss behavior can provide useful runtime context for policy selection.

## Problem Statement

Can lightweight runtime context, initially LLC misses per thousand instructions (MPKI), help identify when one replacement policy may be preferable to another?


## Current implementation

The active configuration, **champsim_config.json**, uses one core and an LLC configured with 2,048 sets, 16 ways, no LLC prefetcher, and DRRIP replacement.

Project instrumentation is in:

- **src/champsim.cc**: logs interval LLC demand MPKI and interval IPC.
- **replacement/drrip/drrip.cc**: records PSEL changes, follower-policy switches, and switch events.
- **replacement/drrip/drrip.h**: defines the 10-bit PSEL counter.

Each interval is approximately one million retired instructions. The MPKI calculation is:

~~~text
MPKI = 1,000 * (delta LLC LOAD misses + delta LLC RFO misses)
       / delta retired instructions
~~~

This counts load and read-for-ownership demand misses. It excludes prefetch and writeback misses. ChampSim emits interval rows for warmup and measurement; use only rows marked phase=Simulation for measured results. 

DRRIP uses SRRIP and BRRIP leader sets and PSEL to select a follower-set policy. In this implementation, follower sets use BRRIP when PSEL is at least 511; otherwise they use SRRIP. PSEL changes and policy switches are separate events: PSEL can change without crossing the selection boundary.

## Policies

- **LRU (Least Recently Used):** evicts the line that has not been used for the longest time.
- **SRRIP (Static Re-Reference Interval Prediction):** uses a 2-bit RRPV per line. New lines are inserted at RRPV 2; hits set RRPV to 0.
- **BRRIP (Bimodal Re-Reference Interval Prediction):** usually inserts at RRPV 3 and occasionally at RRPV 2; hits set RRPV to 0.
- **DRRIP (Dynamic Re-Reference Interval Prediction):** uses set dueling between SRRIP and BRRIP and PSEL to choose the follower-set policy.


## Build

From the repository root in Ubuntu or WSL:

~~~bash
git submodule update --init --recursive
./config.sh champsim_config.json
make 
~~~

If a clean environment needs additional dependencies, follow ChampSim's vcpkg setup instructions. The active configuration selects DRRIP. For a fixed-policy run, save a separate configuration with LLC replacement set to lru, srrip, or brrip, then regenerate and rebuild before running that policy. Keep the exact configuration with its output log.

## Run a trace

Replace the trace path and output filename with the ones for the run:

~~~bash
mkdir -p "results/phase 1/instrumented"
./bin/champsim   --warmup-instructions 250000000   --simulation-instructions 250000000  "/path/to/trace.champsimtrace.xz" 
  
~~~

Run policies sequentially when changing the active generated configuration and binary. Use distinct output filenames so runs do not overwrite one another.

## Results in this checkout

- **results/** also contains earlier raw result files.

- each output is \<policy>_\<trace>.txt

Check each output's configuration, trace, warmup length, and measured length before combining it with another run. These files were collected at different points in the project; verify their settings before comparing them.

## Experimental setup

| Item | Setting |
|---|---|
| Simulator | ChampSim |
| Core count | 1 |
| LLC | 2,048 sets x 16 ways |
| LLC prefetcher | Disabled (no) |
| Warmup | 250 million instructions |
| Measured phase | 250 million instructions |
| Interval size | 1 million retired instructions |
| PSEL | 10-bit saturating counter |
| MPKI miss types | LLC LOAD + RFO |

Workload aliases used in the result filenames:

- **perlbench**: 400.perlbench-41B.champsimtrace.xz
- **ligra**: ligra_PageRankDelta.com-lj.ungraph.gcc_6.3.0_O3.drop_6000M.length_250M.champsimtrace.xz
- **llama**: llama2.c-llama2_7b.1.champsimtrace.gz
- **google**: local alias used in result filenames. Record the exact trace filename and source for each final run; the trace is not stored here.

Trace files are not included. Keep the trace source and exact filename with the experiment record. The current recorded environment is Ubuntu under WSL on a Windows laptop. Record the CPU model, available memory, and Ubuntu version with final results.

### Reading the interval log

An interval row resembles:

~~~text
MPKI phase=Simulation cpu=0 interval=1 instructions=... ipc=... llc_demand_misses=... value=... psel=... psel_changes=... policy=... policy_switches=...
~~~

- **value** is interval LLC demand MPKI.
- **ipc** is interval instructions per cycle.
- **psel** is the PSEL value at the interval sample.
- **psel_changes** counts PSEL value changes during that interval.
- **policy_switches** counts changes in the selected follower policy during that interval.
- **POLICY_SWITCH** rows record phase, interval, instruction position, old and new policy, and PSEL at each switch.

Exclude warmup events from measured policy-switch counts. Do not treat PSEL changes and policy switches as the same metric.



## ChampSim citation

This project builds on ChampSim. Cite:

Gober, N., Chacon, G., Wang, L., Gratz, P. V., Jimenez, D. A., Teran, E., Pugsley, S., and Kim, J. (2022). “The Championship Simulator: Architectural Simulation for Education and Competition.” [arXiv:2210.14324](https://doi.org/10.48550/arXiv.2210.14324).

