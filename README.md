<p align="center">
  <h1 align="center"> ChampSim </h1>
  <p> ChampSim is a trace-based simulator for a microarchitecture study. You can sign up to the public mailing list by sending an empty mail to champsim+subscribe@googlegroups.com. If you have questions about how to use ChampSim, you can often receive a quicker response on the mailing list. Please reserve GitHub Issues for bugs. <p>
</p>

# Clone ChampSim repository
```
git clone https://github.com/ChampSim/ChampSim.git
```

# Compile

ChampSim takes a JSON configuration script. Examine `champsim_config.json` for a fully-specified example. All options described in this file are optional and will be replaced with defaults if not specified. The configuration scrip can also be run without input, in which case an empty file is assumed.
```
$ ./config.sh <configuration file>
$ make
```

# Download DPC-3 trace

Professor Daniel Jimenez at Texas A&M University kindly provided traces for the 3rd Data Prefetching Championship (DPC-3). They can be found here (http://hpca23.cse.tamu.edu/champsim-traces/speccpu/$LINE). A set of traces used for the 2nd Cache Replacement Championship (CRC-2) can be found from this link. (http://bit.ly/2t2nkUj)

# Run simulation

Execute the binary directly.
```
$ bin/champsim --warmup_instructions 200000000 --simulation_instructions 500000000 ~/path/to/traces/600.perlbench_s-210B.champsimtrace.xz
```

The number of warmup and simulation instructions given will be the number of instructions retired. Note that the statistics printed at the end of the simulation include only the simulation phase.

# Add your own branch predictor, data prefetchers, and replacement policy
**Copy an empty template**
```
$ mkdir prefetcher/mypref
$ cp prefetcher/no_l2c/no.cc prefetcher/mypref/mypref.cc
```

**Work on your algorithms with your favorite text editor**
```
$ vim prefetcher/mypref/mypref.cc
```

**Compile and test**
Add your prefetcher to the configuration file.
```
{
    "L2C": {
        "prefetcher": "mypref"
    }
}
```
Note that the example prefetcher is an L2 prefetcher. You might design a prefetcher for a different level.

```
$ ./config.sh <configuration file>
$ make
$ bin/champsim --warmup_instructions 200000000 --simulation_instructions 500000000 600.perlbench_s-210B.champsimtrace.xz
```

# How to create traces

We STRONGLY recommend creating your own traces, covering a wide variety of program types and behaviors.

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
