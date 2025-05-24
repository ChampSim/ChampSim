#include <stdio.h>
#include <stdlib.h>
#include <cassert>
#include <iostream>
#include <cstring>
#include <zlib.h>
#include <cstdlib>
#include "memref.h"
#include "trace_entry.h"

#define REG_STACK_POINTER 6
#define REG_FLAGS 25
#define REG_INSTRUCTION_POINTER 26

using namespace std;

#define NUM_INSTR_DESTINATIONS 2
#define NUM_INSTR_SOURCES 4

#define READ_BUFF_SIZE 12 * 1024
#define WRITE_BUFF_SIZE 64 * 1024

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

int main(int argc, char *argv[])
{
    assert(argc == 3);

    gzFile in = gzopen(argv[1], "rb");

    gzFile out = gzopen(argv[2], "wb");

    if (in == Z_NULL || out == Z_NULL)
    {
        cout << "cannot open file" << endl;
        exit(1);
    }

    trace_entry_t t;
    trace_instr_format_t cs;
    unsigned int sz;
    unsigned long int address;
    int data_in = READ_BUFF_SIZE;

    bool is_first = true;

    char *read_buf = new char[READ_BUFF_SIZE];
    int read_indx = READ_BUFF_SIZE;

    char *write_buf = new char[WRITE_BUFF_SIZE];
    int write_indx = 0;

    while (true)
    {
        int r;

        if (read_indx == data_in)
        {
            r = gzread(in, read_buf, READ_BUFF_SIZE);
            if (r == 0)
            {
                break;
            }

            // cout << r << endl;
            assert(r % 12 == 0);

            data_in = r;

            read_indx = 0;
        }

        if (write_indx == WRITE_BUFF_SIZE)
        {
            r = gzwrite(out, write_buf, WRITE_BUFF_SIZE);

            assert(r == WRITE_BUFF_SIZE);

            write_indx = 0;
        }

        memcpy(&t, read_buf + read_indx, sizeof(t));
        read_indx += sizeof(t);

        switch (t.type)
        {
        case TRACE_TYPE_READ:
        case TRACE_TYPE_PREFETCH:
        case TRACE_TYPE_PREFETCHT0:
        case TRACE_TYPE_PREFETCHT1:
        case TRACE_TYPE_PREFETCHT2:
        case TRACE_TYPE_PREFETCHNTA:
        case TRACE_TYPE_PREFETCH_READ:
        case TRACE_TYPE_PREFETCH_INSTR:
        case TRACE_TYPE_PREFETCH_READ_L1_NT:
        case TRACE_TYPE_PREFETCH_READ_L2_NT:
        case TRACE_TYPE_PREFETCH_READ_L3_NT:
        case TRACE_TYPE_PREFETCH_INSTR_L1:
        case TRACE_TYPE_PREFETCH_INSTR_L1_NT:
        case TRACE_TYPE_PREFETCH_INSTR_L2:
        case TRACE_TYPE_PREFETCH_INSTR_L2_NT:
        case TRACE_TYPE_PREFETCH_INSTR_L3:
        case TRACE_TYPE_PREFETCH_INSTR_L3_NT:
            // Load
            assert(!is_first);

            cs.source_memory[0] = t.addr;

            break;

        case TRACE_TYPE_WRITE:
        case TRACE_TYPE_PREFETCH_WRITE:
        case TRACE_TYPE_PREFETCH_WRITE_L1:
        case TRACE_TYPE_PREFETCH_WRITE_L1_NT:
        case TRACE_TYPE_PREFETCH_WRITE_L2:
        case TRACE_TYPE_PREFETCH_WRITE_L2_NT:
        case TRACE_TYPE_PREFETCH_WRITE_L3:
        case TRACE_TYPE_PREFETCH_WRITE_L3_NT:
            // Store
            assert(!is_first);

            cs.destination_memory[0] = t.addr;

            break;

        case TRACE_TYPE_INSTR:
            // Non-Branch Instr

            if (is_first)
            {

                cs.ip = t.addr;
                cs.is_branch = 0;
                memset(cs.destination_registers, 0, sizeof(cs.destination_registers));
                memset(cs.destination_registers, 0, sizeof(cs.destination_registers));
                memset(cs.source_registers, 0, sizeof(cs.source_registers));
                memset(cs.source_memory, 0, sizeof(cs.source_memory));
                sz = t.size;
                address = t.addr;
                is_first = false;

                break;
            }
            assert(!is_first);

            if (cs.ip + sz != t.addr && cs.ip != t.addr)
            {
                if (cs.is_branch)
                {
                    cs.branch_taken = 1;
                }
            }
            else
            {
                cs.branch_taken = 0;
            }

            memcpy(write_buf + write_indx, &cs, sizeof(cs));
            write_indx += sizeof(cs);

            cs.ip = t.addr;
            cs.is_branch = 0;
            memset(cs.destination_registers, 0, sizeof(cs.destination_registers));
            memset(cs.destination_registers, 0, sizeof(cs.destination_registers));
            memset(cs.source_registers, 0, sizeof(cs.source_registers));
            memset(cs.source_memory, 0, sizeof(cs.source_memory));
            sz = t.size;
            address = t.addr;

            break;

        case TRACE_TYPE_INSTR_DIRECT_JUMP:
        case TRACE_TYPE_INSTR_INDIRECT_JUMP:
        case TRACE_TYPE_INSTR_CONDITIONAL_JUMP:
        case TRACE_TYPE_INSTR_DIRECT_CALL:
        case TRACE_TYPE_INSTR_INDIRECT_CALL:
        case TRACE_TYPE_INSTR_RETURN:
        case TRACE_TYPE_INSTR_SYSENTER:
            // Branch Instr

            if (is_first)
            {
                cs.ip = t.addr;
                cs.is_branch = 1;
                memset(cs.destination_registers, 0, sizeof(cs.destination_registers));
                memset(cs.destination_memory, 0, sizeof(cs.destination_memory));
                memset(cs.source_registers, 0, sizeof(cs.source_registers));
                memset(cs.source_memory, 0, sizeof(cs.source_memory));
                sz = t.size;
                address = t.addr;
                is_first = false;

                if (t.type == TRACE_TYPE_INSTR_DIRECT_JUMP)
                {
                    cs.destination_registers[0] = REG_INSTRUCTION_POINTER; // writes_ip
                }
                else if (t.type == TRACE_TYPE_INSTR_INDIRECT_JUMP)
                {
                    cs.destination_registers[0] = REG_INSTRUCTION_POINTER; // writes_ip
                    cs.source_registers[0] = 99;                           // reads_other
                }
                else if (t.type == TRACE_TYPE_INSTR_CONDITIONAL_JUMP)
                {
                    cs.source_registers[0] = REG_INSTRUCTION_POINTER;      // reads_ip
                    cs.destination_registers[0] = REG_INSTRUCTION_POINTER; // writes_ip
                    cs.source_registers[1] = REG_FLAGS;                    // reads_flag
                }
                else if (t.type == TRACE_TYPE_INSTR_DIRECT_CALL)
                {
                    cs.source_registers[0] = REG_STACK_POINTER;            // reads_sp
                    cs.source_registers[1] = REG_INSTRUCTION_POINTER;      // reads_ip
                    cs.destination_registers[0] = REG_STACK_POINTER;       // writes_sp
                    cs.destination_registers[1] = REG_INSTRUCTION_POINTER; // writes_ip
                }
                else if (t.type == TRACE_TYPE_INSTR_INDIRECT_CALL)
                {
                    cs.source_registers[0] = REG_STACK_POINTER;            // reads_sp
                    cs.source_registers[1] = REG_INSTRUCTION_POINTER;      // reads_ip
                    cs.destination_registers[0] = REG_STACK_POINTER;       // writes_sp
                    cs.destination_registers[1] = REG_INSTRUCTION_POINTER; // writes_ip
                    cs.source_registers[2] = 99;                           // reads_other
                }
                else if (t.type == TRACE_TYPE_INSTR_RETURN)
                {
                    cs.source_registers[0] = REG_STACK_POINTER;            // reads_sp
                    cs.destination_registers[0] = REG_STACK_POINTER;       // writes_sp
                    cs.destination_registers[1] = REG_INSTRUCTION_POINTER; // writes_ip
                }
                else
                {
                    cs.destination_registers[1] = REG_INSTRUCTION_POINTER; // writes_ip
                }

                break;
            }

            assert(!is_first);

            if (cs.ip + sz != t.addr && cs.ip != t.addr)
            {
                if (cs.is_branch)
                {
                    cs.branch_taken = 1;
                }
            }
            else
            {
                cs.branch_taken = 0;
            }

            memcpy(write_buf + write_indx, &cs, sizeof(cs));
            write_indx += sizeof(cs);

            cs.ip = t.addr;
            cs.is_branch = 1;
            memset(cs.destination_registers, 0, sizeof(cs.destination_registers));
            memset(cs.destination_memory, 0, sizeof(cs.destination_memory));
            memset(cs.source_registers, 0, sizeof(cs.source_registers));
            memset(cs.source_memory, 0, sizeof(cs.source_memory));
            sz = t.size;
            address = t.addr;

            if (t.type == TRACE_TYPE_INSTR_DIRECT_JUMP)
            {
                cs.destination_registers[0] = REG_INSTRUCTION_POINTER; // writes_ip
            }
            else if (t.type == TRACE_TYPE_INSTR_INDIRECT_JUMP)
            {
                cs.destination_registers[0] = REG_INSTRUCTION_POINTER; // writes_ip
                cs.source_registers[0] = 99;                           // reads_other
            }
            else if (t.type == TRACE_TYPE_INSTR_CONDITIONAL_JUMP)
            {
                cs.source_registers[0] = REG_INSTRUCTION_POINTER;      // reads_ip
                cs.destination_registers[0] = REG_INSTRUCTION_POINTER; // writes_ip
                cs.source_registers[1] = REG_FLAGS;                    // reads_flag
            }
            else if (t.type == TRACE_TYPE_INSTR_DIRECT_CALL)
            {
                cs.source_registers[0] = REG_STACK_POINTER;            // reads_sp
                cs.source_registers[1] = REG_INSTRUCTION_POINTER;      // reads_ip
                cs.destination_registers[0] = REG_STACK_POINTER;       // writes_sp
                cs.destination_registers[1] = REG_INSTRUCTION_POINTER; // writes_ip
            }
            else if (t.type == TRACE_TYPE_INSTR_INDIRECT_CALL)
            {
                cs.source_registers[0] = REG_STACK_POINTER;            // reads_sp
                cs.source_registers[1] = REG_INSTRUCTION_POINTER;      // reads_ip
                cs.destination_registers[0] = REG_STACK_POINTER;       // writes_sp
                cs.destination_registers[1] = REG_INSTRUCTION_POINTER; // writes_ip
                cs.source_registers[2] = 99;                           // reads_other
            }
            else if (t.type == TRACE_TYPE_INSTR_RETURN)
            {
                cs.source_registers[0] = REG_STACK_POINTER;            // reads_sp
                cs.destination_registers[0] = REG_STACK_POINTER;       // writes_sp
                cs.destination_registers[1] = REG_INSTRUCTION_POINTER; // writes_ip
            }
            else
            {
                cs.destination_registers[1] = REG_INSTRUCTION_POINTER; // writes_ip
            }

            break;

        case TRACE_TYPE_INSTR_BUNDLE:
            // Instruction Bundle
            cout << "Instruction Bundle Type" << endl;
            assert(0);
            break;

        case TRACE_TYPE_INSTR_FLUSH:
        case TRACE_TYPE_INSTR_FLUSH_END:
        case TRACE_TYPE_DATA_FLUSH:
        case TRACE_TYPE_DATA_FLUSH_END:
            // Flush
            cout << "Flush Type" << endl;
            assert(0);
            break;

        case TRACE_TYPE_THREAD:
            // Beginning of Thread
            break;

        case TRACE_TYPE_THREAD_EXIT:
            // Thread Exit
            break;

        case TRACE_TYPE_PID:
            // Process of current thread
            break;

        case TRACE_TYPE_HEADER:

            break;

        case TRACE_TYPE_FOOTER:

            break;

        case TRACE_TYPE_HARDWARE_PREFETCH:
            cout << "Hardware Prefetch Type" << endl;
            assert(0);
            break;

        case TRACE_TYPE_MARKER:
            // Marker
            switch (t.size)
            {
            case TRACE_MARKER_TYPE_KERNEL_EVENT:
                if (cs.ip + sz != t.addr && cs.ip != t.addr)
                {
                    // assert(cs.is_branch);
                    if (cs.is_branch)
                    {
                        cs.branch_taken = 1;
                    }
                }
                else
                {
                    cs.branch_taken = 0;
                }

                is_first = true;
                break;

            case TRACE_MARKER_TYPE_KERNEL_XFER:
                if (cs.ip + sz != t.addr && cs.ip != t.addr)
                {
                    // assert(cs.is_branch);
                    if (cs.is_branch)
                    {
                        cs.branch_taken = 1;
                    }
                }
                else
                {
                    cs.branch_taken = 0;
                }

                is_first = true;
                break;

            default:
                break;
            }

            break;

        case TRACE_TYPE_ENCODING:
            cout << "Encoding Type" << endl;
            assert(0);
            break;

        case TRACE_TYPE_INSTR_NO_FETCH:

            if (is_first)
            {
                cs.ip = t.addr;
                cs.is_branch = 0;
                memset(cs.destination_registers, 0, sizeof(cs.destination_registers));
                memset(cs.destination_memory, 0, sizeof(cs.destination_memory));
                memset(cs.source_registers, 0, sizeof(cs.source_registers));
                memset(cs.source_memory, 0, sizeof(cs.source_memory));
                sz = t.size;
                address = t.addr;
                is_first = false;
                break;
            }

            assert(!is_first);

            if (cs.ip + sz != t.addr && cs.ip != t.addr)
            {
                // assert(cs.is_branch);
                if (cs.is_branch)
                {
                    cs.branch_taken = 1;
                }
            }
            else
            {
                cs.branch_taken = 0;
            }

            memcpy(write_buf + write_indx, &cs, sizeof(cs));
            write_indx += sizeof(cs);

            cs.ip = t.addr;
            cs.is_branch = 0;
            memset(cs.destination_registers, 0, sizeof(cs.destination_registers));
            memset(cs.destination_memory, 0, sizeof(cs.destination_memory));
            memset(cs.source_registers, 0, sizeof(cs.source_registers));
            memset(cs.source_memory, 0, sizeof(cs.source_memory));
            sz = t.size;
            address = t.addr;

            break;

        case TRACE_TYPE_INSTR_MAYBE_FETCH:
            cout << "Maybe Fetch Type" << endl;
            assert(0);
            break;

        default:
            cout << "Exception Hit. Invalid Type= " << t.type << endl;
            assert(0);
            break;
        }
    }

    int r = gzwrite(out, write_buf, write_indx);
    assert(r == write_indx);

    gzclose(in);
    gzclose(out);
}