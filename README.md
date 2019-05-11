<p align="center">
  <h1 align="center"> ChampSim </h1>
  <p> ChampSim is a trace-based simulator for a microarchitecture study. You can sign up to the public mailing list by sending an empty mail to champsim+subscribe@googlegroups.com. Traces for the 3rd Data Prefetching Championship (DPC-3) can be found from here (https://dpc3.compas.cs.stonybrook.edu/?SW_IS). A set of traces used for the 2nd Cache Replacement Championship (CRC-2) can be found from this link. (http://bit.ly/2t2nkUj) <p>
</p>

# Clone ChampSim repository
```
git clone https://github.com/ChampSim/ChampSim.git
```

# Compile

ChampSim takes five parameters: Branch predictor, L1D prefetcher, L2C prefetcher, LLC replacement policy, and the number of cores. 
For example, the following builds a single-core processor with bimodal branch predictor, next-line L1/L2/LL data prefetchers, and the baseline LRU replacement policy for the LLC.
```
$ ./config.sh -b branch/bimodal.b_pred\
              -1 prefetcher/next_line.l1d_pref\
              -2 prefetcher/next_line.l2c_pref\
              -3 prefetcher/next_line.llc_pref\
              -r replacement/lru.llc_repl\
              -n 1
```
More options are available. Run `./config.sh -h` for more information. Additionally, the configuration script supports a configuration file of key-value pairs. An example, `example_config.txt`, is given.
After the configuration step, ChampSim can be built with `make`. If the configuration script is re-run with new parameters, a `make clean` may be required to commit the changes.

# Download DPC-3 trace

Professor Daniel Jimenez at Texas A&M University kindly provided traces for DPC-3. Use the following script to download these traces (~20GB size and max simpoint only).
```
$ cd scripts

$ ./download_dpc3_traces.sh
```

# Run simulation

Execute `run_champsim.sh` with proper input arguments. The default `TRACE_DIR` in `run_champsim.sh` is set to `$PWD/dpc3_traces`. <br>

* Single-core simulation: Run simulation with `run_champsim.sh` script.

```
$ ./${BINARY} -warmup_instructions ${N_WARM}000000 -simulation_instructions ${N_SIM}000000 ${OPTION} -traces ${TRACE}

Example:
$ ./champsim -warmup_instructions 1 -simulation_instructions 10 -traces dpc3_traces/400.perlbench-41B.champsimtrace.xz

${BINARY}: name of the compiled champsim binary (bin/champsim)
${N_WARM}: number of instructions for warmup (1 million)
${N_SIM}:  number of instructinos for detailed simulation (10 million)
${OPTION}: extra option for "-low_bandwidth" (src/main.cc)
${TRACE}: trace name (400.perlbench-41B.champsimtrace.xz)
```
Simulation results will be outputted to the standard output, where they can be piped to a file of your choosing.<br>

* Multi-core simulation is the same as above, but one trace per core must be specified. <br>

# Add your own branch predictor, data prefetchers, and replacement policy
**Copy an empty template**
```
$ cp branch/branch_predictor.cc prefetcher/mybranch.bpred
$ cp prefetcher/l1d_prefetcher.cc prefetcher/mypref.l1d_pref
$ cp prefetcher/l2c_prefetcher.cc prefetcher/mypref.l2c_pref
$ cp prefetcher/llc_prefetcher.cc prefetcher/mypref.llc_pref
$ cp replacement/llc_replacement.cc replacement/myrepl.llc_repl
```

**Work on your algorithms with your favorite text editor**
```
$ vim branch/mybranch.bpred
$ vim prefetcher/mypref.l1d_pref
$ vim prefetcher/mypref.l2c_pref
$ vim prefetcher/mypref.llc_pref
$ vim replacement/myrepl.llc_repl
```

**Compile and test**
```
$ ./config.sh -b mybranch -1 mypref -2 mypref -3 mypref -r myrepl -n 1 -o mybranch-mypref-mypref-mypref-myrepl-1core
$ make
$ ./mybranch-mypref-mypref-mypref-myrepl-1core -warmup_instructions 1 -simulation_instructions 10 -traces dpc3_traces/bzip2_183B
```

# How to create traces

We have included only 4 sample traces, taken from SPEC CPU 2006. These 
traces are short (10 million instructions), and do not necessarily cover the range of behaviors your 
replacement algorithm will likely see in the full competition trace list (not
included).  We STRONGLY recommend creating your own traces, covering
a wide variety of program types and behaviors.

The included Pin Tool champsim_tracer.cpp can be used to generate new traces.
We used Pin 3.2 (pin-3.2-81205-gcc-linux), and it may require 
installing libdwarf.so, libelf.so, or other libraries, if you do not already 
have them. Please refer to the Pin documentation (https://software.intel.com/sites/landingpage/pintool/docs/81205/Pin/html/)
for working with Pin 3.2.

Get this version of Pin:
```
wget http://software.intel.com/sites/landingpage/pintool/downloads/pin-3.2-81205-gcc-linux.tar.gz
```

**Use the Pin tool like this**
```
pin -t obj-intel64/champsim_tracer.so -- <your program here>
```

The tracer has three options you can set:
```
-o
Specify the output file for your trace.
The default is default_trace.champsim

-s <number>
Specify the number of instructions to skip in the program before tracing begins.
The default value is 0.

-t <number>
The number of instructions to trace, after -s instructions have been skipped.
The default value is 1,000,000.
```
For example, you could trace 200,000 instructions of the program ls, after
skipping the first 100,000 instructions, with this command:
```
pin -t obj/champsim_tracer.so -o traces/ls_trace.champsim -s 100000 -t 200000 -- ls
```
Traces created with the champsim_tracer.so are approximately 64 bytes per instruction,
but they generally compress down to less than a byte per instruction using xz compression.

# Evaluate Simulation

ChampSim measures the IPC (Instruction Per Cycle) value as a performance metric. <br>
There are some other useful metrics printed out at the end of simulation. <br>

Good luck and be a champion! <br>
