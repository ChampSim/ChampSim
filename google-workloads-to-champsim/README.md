# Google Workload Traces to Champsim Wrapper Script

## Compile
Use the following command to compile: 

`g++ google-workload-to-champsim.cpp -lz` 

## Run
Use the following command to run the executable:

`./a.out <trace>`

`<trace>` should be a google workload trace file in compressed form with `.gz` extension. Google workload traces can be obtained [here](https://dynamorio.org/google_workload_traces.html).

A new trace file compatible with champsim will be created. "champsim-" will be prefixed to the original trace file's name. This trace file will also be in compressed form with `.gz` extension.

## About Wrapper Script

### Google Workload Trace Format
Google Workload traces are stored as records, each record being of type `trace_entry_t` and size 12 bytes. Following is the structure of `trace_entry_t`:

```c
struct _trace_entry_t {
    unsigned short type;
    unsigned short size;
    union {
        addr_t addr; 
        unsigned char length[sizeof(addr_t)];
        unsigned char encoding[sizeof(addr_t)];
    };
}
```

1. `type` : the type of entry. Instruction/Memory-Access/Marker/etc.
2. `size` : memory reference size or instruction length
3. `addr` : mem ref addr, instr pc, tid, pid, marker val
4. `length` : not applicable in our context
5. `encoding` : not applicable in our context

Types of records can be broadly classified into following:

1. Non-Branch Instructions
2. Branch Instructions
3. Memory Load
4. Memory Store
5. Marker
6. Other

> Google Workload traces guarantee that a memory access record will immediately be preceeded by its corresponding instruction

### Champsim Trace Format
Champsim traces are also stored as records, each record being of type `trace_instr_format_t` and size 64 bytes. Following is the structure of `trace_instr_format_t`:

```c
typedef struct trace_instr_format
{
    unsigned long long int ip; // instruction pointer (program counter) value

    unsigned char is_branch;    // is this branch
    unsigned char branch_taken; // if so, is this taken

    unsigned char destination_registers[NUM_INSTR_DESTINATIONS]; // output registers
    unsigned char source_registers[NUM_INSTR_SOURCES];           // input registers

    unsigned long long int destination_memory[NUM_INSTR_DESTINATIONS]; // output memory
    unsigned long long int source_memory[NUM_INSTR_SOURCES];           // input memory
} trace_instr_format_t;
```

### Conversion

Converting of Non-Branch instructions to champsim format is straightforward. We just copy instruction pointer value and set `is_branch=0` and `branch_taken=0`.

Converting of Branch instructions is a bit involved. Again we copy instruction pointer and set `is_branch=1`. To set `branch_taken` we need the instruction pointer of next instruction. If the `ip` of next instruction is equal to `ip` of current instruction plus the size of instruction, then branch is not taken. So we set `branch_taken=0`. Otherwise we set `branch_taken=1`. Now branch instructions can be of following types:
1. Direct Jump
2. Indirect Jump
3. Conditional Jump
4. Direct Call
5. Indirect Call
6. Return
7. Sysenter (x86 specific instruction)

The way champsim distiguishes these types is by values in `destination_registers` and `source_registers`. So according to the type of branch instruction, we need to set appropriate values of these registers. 

> Sysenter branch instruction will show up in BRANCH_OTHER type in champsim

For memory accesses, we just copy the address of memory in either `destination_memory` or `source_memory` for store and load respectively.

For markers, we can ignore almost all types except `TRACE_MARKER_TYPE_KERNEL_EVENT` and `TRACE_MARKER_TYPE_KERNEL_XFER` as the rest just provide additional information about trace which champsim does not use.






