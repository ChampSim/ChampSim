#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

#include "circular_buffer.hpp"

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

class LSQ_ENTRY;

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
             event_cycle = 0;

    bool is_branch = 0,
         is_memory = 0,
         branch_taken = 0,
         branch_mispredicted = 0,
         source_added[NUM_INSTR_SOURCES] = {},
         destination_added[NUM_INSTR_DESTINATIONS_SPARC] = {};

    uint16_t asid = std::numeric_limits<uint16_t>::max();

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

    // these are indices of instructions in the ROB that depend on me
    std::vector<champsim::circular_buffer<ooo_model_instr>::iterator> registers_instrs_depend_on_me, memory_instrs_depend_on_me;

    // memory addresses that may cause dependencies between instructions
    uint64_t instruction_pa = 0;
    uint64_t destination_memory[NUM_INSTR_DESTINATIONS_SPARC] = {}; // output memory
    uint64_t source_memory[NUM_INSTR_SOURCES] = {}; // input memory

    std::array<std::vector<LSQ_ENTRY>::iterator, NUM_INSTR_SOURCES> lq_index = {};
    std::array<std::vector<LSQ_ENTRY>::iterator, NUM_INSTR_DESTINATIONS_SPARC> sq_index = {};

    ooo_model_instr() = default;

    explicit ooo_model_instr(input_instr instr) : ip(instr.ip), is_branch(instr.is_branch), branch_taken(instr.branch_taken)
    {
        std::copy(std::begin(instr.destination_registers), std::end(instr.destination_registers), std::begin(this->destination_registers));
        std::copy(std::begin(instr.destination_memory), std::end(instr.destination_memory), std::begin(this->destination_memory));
        std::copy(std::begin(instr.source_registers), std::end(instr.source_registers), std::begin(this->source_registers));
        std::copy(std::begin(instr.source_memory), std::end(instr.source_memory), std::begin(this->source_memory));
    }

    explicit ooo_model_instr(cloudsuite_instr instr) : ip(instr.ip), is_branch(instr.is_branch), branch_taken(instr.branch_taken), asid((static_cast<uint16_t>(instr.asid[1]) << 8) + instr.asid[0])
    {
        std::copy(std::begin(instr.destination_registers), std::end(instr.destination_registers), std::begin(this->destination_registers));
        std::copy(std::begin(instr.destination_memory), std::end(instr.destination_memory), std::begin(this->destination_memory));
        std::copy(std::begin(instr.source_registers), std::end(instr.source_registers), std::begin(this->source_registers));
        std::copy(std::begin(instr.source_memory), std::end(instr.source_memory), std::begin(this->source_memory));
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

