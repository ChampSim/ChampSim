#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <cstdint>
#include <iostream>
#include <limits>

// instruction format
#define NUM_INSTR_DESTINATIONS_SPARC 4
#define NUM_INSTR_DESTINATIONS 2
#define NUM_INSTR_SOURCES 4

// special registers that help us identify branches
#define REG_STACK_POINTER 6
#define REG_FLAGS 25
#define REG_INSTRUCTION_POINTER 26

// branch types
#define NOT_BRANCH           0
#define BRANCH_DIRECT_JUMP   1
#define BRANCH_INDIRECT      2
#define BRANCH_CONDITIONAL   3
#define BRANCH_DIRECT_CALL   4
#define BRANCH_INDIRECT_CALL 5
#define BRANCH_RETURN        6
#define BRANCH_OTHER         7

#include "set.h"

#include <limits>

struct input_instr {
    // instruction pointer or PC (Program Counter)
    uint64_t ip = 0;

    // branch info
    uint8_t is_branch = 0;
    uint8_t branch_taken = 0;

    uint8_t destination_registers[NUM_INSTR_DESTINATIONS] = {}; // output registers
    uint8_t source_registers[NUM_INSTR_SOURCES] = {}; // input registers

    uint64_t destination_memory[NUM_INSTR_DESTINATIONS] = {}; // output memory
    uint64_t source_memory[NUM_INSTR_SOURCES] = {}; // input memory
};

struct cloudsuite_instr {
    // instruction pointer or PC (Program Counter)
    uint64_t ip = 0;

    // branch info
    uint8_t is_branch = 0;
    uint8_t branch_taken = 0;

    uint8_t destination_registers[NUM_INSTR_DESTINATIONS_SPARC] = {}; // output registers
    uint8_t source_registers[NUM_INSTR_SOURCES] = {}; // input registers

    uint64_t destination_memory[NUM_INSTR_DESTINATIONS_SPARC] = {}; // output memory
    uint64_t source_memory[NUM_INSTR_SOURCES] = {}; // input memory

    uint8_t asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()};
};

struct ooo_model_instr {
    uint64_t instr_id = 0,
             ip = 0,
             fetch_producer = 0,
             producer_id = 0,
             event_cycle = 0,
             cycle_add_to_lsq = 0;

    bool is_branch = 0,
         is_memory = 0,
         branch_taken = 0,
         branch_mispredicted = 0,
         source_added[NUM_INSTR_SOURCES] = {},
         destination_added[NUM_INSTR_DESTINATIONS_SPARC] = {},
         is_producer = 0,
         is_consumer = 0,
         reg_RAW_producer = 0,
         reg_ready = 0,
         mem_ready = 0,
         reg_RAW_checked[NUM_INSTR_SOURCES] = {};

    uint8_t asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()};

    uint8_t branch_type = NOT_BRANCH;
    uint64_t branch_target = 0;

    uint8_t translated = 0,
            fetched = 0,
            decoded = 0,
            scheduled = 0,
            executed = 0;
    int num_reg_ops = 0, num_mem_ops = 0, num_reg_dependent = 0;

    uint8_t destination_registers[NUM_INSTR_DESTINATIONS_SPARC] = {}; // output registers

    uint8_t source_registers[NUM_INSTR_SOURCES] = {}; // input registers 

    fastset
	registers_instrs_depend_on_me, registers_index_depend_on_me[NUM_INSTR_SOURCES];


    // memory addresses that may cause dependencies between instructions
    uint64_t instruction_pa = 0, data_pa = 0, virtual_address = 0, physical_address = 0;
    uint64_t destination_memory[NUM_INSTR_DESTINATIONS_SPARC] = {}; // output memory
    uint64_t source_memory[NUM_INSTR_SOURCES] = {}; // input memory
    //int source_memory_outstanding[NUM_INSTR_SOURCES];  // a value of 2 here means the load hasn't been issued yet, 1 means it has been issued, but not returned yet, and 0 means it has returned

    // keep around a record of what the original virtual addresses were
    uint64_t destination_virtual_address[NUM_INSTR_DESTINATIONS_SPARC] = {};
    uint64_t source_virtual_address[NUM_INSTR_SOURCES] = {};

    // these are instruction ids of other instructions in the window
    //uint32_t memory_instrs_i_depend_on[NUM_INSTR_SOURCES];

    // these are indices of instructions in the ROB that depend on me
    fastset memory_instrs_depend_on_me;

    uint32_t lq_index[NUM_INSTR_SOURCES],
             sq_index[NUM_INSTR_DESTINATIONS_SPARC],
             forwarding_index[NUM_INSTR_DESTINATIONS_SPARC] = {};

    ooo_model_instr() {
        std::fill(std::begin(lq_index), std::end(lq_index), std::numeric_limits<uint32_t>::max());
        std::fill(std::begin(sq_index), std::end(sq_index), std::numeric_limits<uint32_t>::max());
    };

    ooo_model_instr(uint8_t cpu, input_instr instr) : ooo_model_instr()
    {
        std::copy(std::begin(instr.destination_registers), std::end(instr.destination_registers), std::begin(this->destination_registers));
        std::copy(std::begin(instr.destination_memory), std::end(instr.destination_memory), std::begin(this->destination_memory));
        std::copy(std::begin(instr.source_registers), std::end(instr.source_registers), std::begin(this->source_registers));
        std::copy(std::begin(instr.source_memory), std::end(instr.source_memory), std::begin(this->source_memory));

        this->ip = instr.ip;
        this->is_branch = instr.is_branch;
        this->branch_taken = instr.branch_taken;

        asid[0] = cpu;
        asid[1] = cpu;
    }

    ooo_model_instr(uint8_t cpu, cloudsuite_instr instr) : ooo_model_instr()
    {
        std::copy(std::begin(instr.destination_registers), std::end(instr.destination_registers), std::begin(this->destination_registers));
        std::copy(std::begin(instr.destination_memory), std::end(instr.destination_memory), std::begin(this->destination_memory));
        std::copy(std::begin(instr.source_registers), std::end(instr.source_registers), std::begin(this->source_registers));
        std::copy(std::begin(instr.source_memory), std::end(instr.source_memory), std::begin(this->source_memory));

        this->ip = instr.ip;
        this->is_branch = instr.is_branch;
        this->branch_taken = instr.branch_taken;

        std::copy(std::begin(instr.asid), std::begin(instr.asid), std::begin(this->asid));
    }

    void print_instr()
    {
        std::cout << "*** " << instr_id << " ***" << std::endl;
        std::cout << std::hex << "0x" << (uint64_t)ip << std::dec << std::endl;
        std::cout << (uint32_t)is_branch << " " << (uint32_t)branch_taken << std::endl;
        for(uint32_t i=0; i<NUM_INSTR_SOURCES; i++)
        {
            std::cout << (uint32_t)source_registers[i] << " ";
        }
        std::cout << std::endl;
        for(uint32_t i=0; i<NUM_INSTR_SOURCES; i++)
        {
            std::cout << std::hex << "0x" << (uint32_t)source_memory[i] << std::dec << " ";
        }
        std::cout << std::endl;
        for(uint32_t i=0; i<NUM_INSTR_DESTINATIONS; i++)
        {
            std::cout << (uint32_t)destination_registers[i] << " ";
        }
        std::cout << std::endl;
        for(uint32_t i=0; i<NUM_INSTR_DESTINATIONS; i++)
        {
            std::cout << std::hex << "0x" << (uint32_t)destination_memory[i] << std::dec << " ";
        }
        std::cout << std::endl;
        std::cout << std::endl;
    }
};

#endif

