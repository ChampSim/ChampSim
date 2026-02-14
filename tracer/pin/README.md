# Intel PIN tracer

The included PIN tool `champsim_tracer.cpp` can be used to generate new traces.
It has been tested (April 2022) using PIN 3.22.

## Download and install PIN

Download the source of PIN from Intel's website, then build it in a location of your choice.

    wget https://software.intel.com/sites/landingpage/pintool/downloads/pin-3.22-98547-g7a303a835-gcc-linux.tar.gz
    tar zxf pin-3.22-98547-g7a303a835-gcc-linux.tar.gz
    cd pin-3.22-98547-g7a303a835-gcc-linux/source/tools
    make
    export PIN_ROOT=/your/path/to/pin

## Building the tracer

The provided makefile will generate `obj-intel64/champsim_tracer.so`.

    make
    $PIN_ROOT/pin -t obj-intel64/champsim_tracer.so -- <your program here>

The tracer has three options you can set:
```
-o
Specify the output file for your trace.
The default is default_trace.champsim

-s <number>
Specify the number of instructions to skip in the program before tracing begins.
The default value is 0.

-t <number>
The cumulative number of instructions to trace.
The default value is 1,000,000.

-start_symbol <symbol_name>
Symbol name to start tracing (e.g., 'main' or '_Z4testv'). If specified `-s` argument is ignored.

-stop_symbol <symbol_name>
"Symbol name to stop tracing (optional)"
```
For example, you could trace 200,000 instructions of the program ls, after skipping the first 100,000 instructions, with this command:

    pin -t obj/champsim_tracer.so -o traces/ls_trace.champsim -s 100000 -t 200000 -- ls

Or if you are interested only in a particular section of the executible you could add symbols marking the beginning and end of the region. For example, [`symbol_trace_example.cpp`](symbol_trace_example.cpp) used like the following:

```bash
# compile the program to `a.out`
g++ symbol_trace_example.cpp
# check the symbols are present in the output binary
nm a.out | grep champsim
# run the tracer with the symbols demarcating where to start and stop
pin -t obj/champsim_tracer.so -o traces/symbol_trace_example.champsim -start_symbol __champsim_start_trace -stop_symbol __champsim_start_trace -- ./a.out
```

Traces created with the champsim_tracer.so are approximately 64 bytes per instruction, but they generally compress down to less than a byte per instruction using xz compression.

