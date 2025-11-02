# Intel Pin tracer

The included Pin tool `champsim_tracer.cpp` can be used to generate new traces.
It has been tested (November 2025) using Pin 3.31 and GCC 14.

## Download and install Pin

Download the source of Pin from Intel's website, then build it in a location of your choice.

    wget https://software.intel.com/sites/landingpage/pintool/downloads/pin-3.30-98830-g1d7b601b3-gcc-linux.tar.gz
    tar zxf pin-3.30-98830-g1d7b601b3-gcc-linux.tar.gz
    cd pin-3.30-98830-g1d7b601b3-gcc-linux/source/tools
    make
    export PIN_ROOT=/your/path/to/pin

## Building the tracer

*2025/11/01: Note that the build process has changed due to the addition of bzip2 compression support.*

### Building libbz2 for the Pin runtime
Intel Pin requires all libraries used by a Pin tool to be built and linked against its PinCRT runtime.
(See Intel's PinCRT PDF file from the [Pin downloads page](https://www.intel.com/content/www/us/en/developer/articles/tool/pin-a-binary-instrumentation-tool-downloads.html) for more information)

To build the libbz2 dependency (Note that these instructions assume you have already run vcpkg install during initial repository setup):

    export PIN_ROOT=/your/path/to/pin
    ln -s ../../vcpkg/buildtrees/bzip2/src/bzip2* bzip2
    cd bzip2
    make libbz2.a LDFLAGS='-nostdlib -lc-dynamic -lm-dynamic -lc++ -lc++abi -lpindwarf \
    -ldwarf -L$(PIN_ROOT)/intel64/runtime/pincrt -L$(PIN_ROOT)/intel64/lib' \
    CFLAGS='-D__PIN__=1 -DPIN_CRT=1 -DTARGET_LINUX -DTARGET_IA32E -DHOST_IA32E \
    -funwind-tables -fno-stack-protector -fasynchronous-unwind-tables \
    -fomit-frame-pointer -fno-strict-aliasing -fPIC -O2 -D_FILE_OFFSET_BITS=64 \
    -isystem $(PIN_ROOT)/extras/cxx/include \
    -isystem $(PIN_ROOT)/extras/crt/include \
    -isystem $(PIN_ROOT)/extras/crt/include/arch-x86_64 \
    -isystem $(PIN_ROOT)/extras/crt/include/kernel/uapi \
    -isystem $(PIN_ROOT)/extras/crt/include/kernel/uapi/asm-x86 \
    -I$(PIN_ROOT)/source/include/pin \
    -I$(PIN_ROOT)/source/include/pin/gen \
    -I$(PIN_ROOT)/extras/components/include \
    -I$(PIN_ROOT)/extras/xed-intel64/include/xed'

### Building the Pin tool

The provided makefile will generate `obj-intel64/champsim_tracer.so`.

    export PIN_ROOT=/your/path/to/pin
    make


# Usage

    $PIN_ROOT/pin -t obj-intel64/champsim_tracer.so -- <your program here>

The tracer has four options you can set:

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

-b
Enables bzip2 compression of the trace. Note that this appends ".bz2" to the output file name.
The default behavior is to disable compression.
```

For example, you could trace 200,000 instructions of the program ls, after skipping the first 100,000 instructions, with this command:

    pin -t obj/champsim_tracer.so -o traces/ls_trace.champsim -s 100000 -t 200000 -- ls

The following generates a compressed trace file 'traces/ls_trace.champsim.bz2':

    pin -t obj/champsim_tracer.so -o traces/ls_trace.champsim -s 100000 -t 200000 -b -- ls

Traces created with the champsim_tracer.so are approximately 64 bytes per instruction, but they generally compress down to less than a byte per instruction using xz compression.

