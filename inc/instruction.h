#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <array>
#include <limits>

// instruction format
#define ROB_SIZE 352
#define LQ_SIZE 128
#define SQ_SIZE 72
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

class input_instr {
  public:

    // instruction pointer or PC (Program Counter)
    uint64_t ip;

    // branch info
    uint8_t is_branch;
    uint8_t branch_taken;

    uint8_t destination_registers[NUM_INSTR_DESTINATIONS]; // output registers
    uint8_t source_registers[NUM_INSTR_SOURCES]; // input registers

    uint64_t destination_memory[NUM_INSTR_DESTINATIONS]; // output memory
    uint64_t source_memory[NUM_INSTR_SOURCES]; // input memory

    input_instr() {
        ip = 0;
        is_branch = 0;
        branch_taken = 0;

        for (uint32_t i=0; i<NUM_INSTR_SOURCES; i++) {
            source_registers[i] = 0;
            source_memory[i] = 0;
        }

        for (uint32_t i=0; i<NUM_INSTR_DESTINATIONS; i++) {
            destination_registers[i] = 0;
            destination_memory[i] = 0;
        }
    };
};

class cloudsuite_instr {
  public:

    // instruction pointer or PC (Program Counter)
    uint64_t ip;

    // branch info
    uint8_t is_branch;
    uint8_t branch_taken;

    uint8_t destination_registers[NUM_INSTR_DESTINATIONS_SPARC]; // output registers
    uint8_t source_registers[NUM_INSTR_SOURCES]; // input registers

    uint64_t destination_memory[NUM_INSTR_DESTINATIONS_SPARC]; // output memory
    uint64_t source_memory[NUM_INSTR_SOURCES]; // input memory

    uint8_t asid[2];

    cloudsuite_instr() {
        ip = 0;
        is_branch = 0;
        branch_taken = 0;

        for (uint32_t i=0; i<NUM_INSTR_SOURCES; i++) {
            source_registers[i] = 0;
            source_memory[i] = 0;
        }

        for (uint32_t i=0; i<NUM_INSTR_DESTINATIONS_SPARC; i++) {
            destination_registers[i] = 0;
            destination_memory[i] = 0;
        }

        asid[0] = UINT8_MAX;
        asid[1] = UINT8_MAX;
    };
};

class ooo_model_instr {
  public:
    uint64_t instr_id,
             ip,
             fetch_producer,
             producer_id,
             translated_cycle,
             fetched_cycle,
             execute_begin_cycle,
             retired_cycle,
             event_cycle;

    uint8_t is_branch,
            is_memory,
            branch_taken,
            branch_mispredicted,
            branch_prediction_made,
            translated,
            data_translated;

    std::array<uint8_t, NUM_INSTR_SOURCES> source_added;
    std::array<uint8_t, NUM_INSTR_DESTINATIONS_SPARC> destination_added;

    uint8_t is_producer,
            is_consumer,
            reg_RAW_producer,
            reg_ready,
            mem_ready;

    std::array<uint8_t, 2> asid;
    std::array<uint8_t, NUM_INSTR_SOURCES> reg_RAW_checked;

    uint8_t branch_type;
    uint64_t branch_target;

    uint32_t fetched, scheduled;
    int num_reg_ops, num_mem_ops, num_reg_dependent;

    // executed bit is set after all dependencies are eliminated and this instr is chosen on a cycle, according to EXEC_WIDTH
    int executed;

    std::array<uint8_t, NUM_INSTR_DESTINATIONS_SPARC> destination_registers; // output registers

    std::array<uint8_t, NUM_INSTR_SOURCES> source_registers; // input registers 

    // these are instruction ids of other instructions in the window
    //std::array<int64_t, NUM_INSTR_SOURCES> registers_instrs_i_depend_on;
    // these are indices of instructions in the window that depend on me
    //std::array<uint8_t, ROB_SIZE> registers_instrs_depend_on_me;
    //std::array<std::array<uint8_t, ROB_SIZE>, NUM_INSTR_SOURCES> registers_index_depend_on_me[ROB_SIZE][NUM_INSTR_SOURCES];
    fastset registers_instrs_depend_on_me;
    std::array<fastset, NUM_INSTR_SOURCES> registers_index_depend_on_me;


    // memory addresses that may cause dependencies between instructions
    uint64_t instruction_pa, data_pa, virtual_address, physical_address;
    std::array<uint64_t, NUM_INSTR_DESTINATIONS_SPARC> destination_memory; // output memory
    std::array<uint64_t, NUM_INSTR_SOURCES> source_memory; // input memory
    //std::array<int, NUM_INSTR_SOURCES> source_memory_outstanding;  // a value of 2 here means the load hasn't been issued yet, 1 means it has been issued, but not returned yet, and 0 means it has returned

    // keep around a record of what the original virtual addresses were
    std::array<uint64_t, NUM_INSTR_DESTINATIONS_SPARC> destination_virtual_address;
    std::array<uint64_t, NUM_INSTR_SOURCES> source_virtual_address;

    // these are instruction ids of other instructions in the window
    //std::array<uint32_t, NUM_INSTR_SOURCES> memory_instrs_i_depend_on;

    // these are indices of instructions in the ROB that depend on me
    //std::array<uint8_t, ROB_SIZE> memory_instrs_depend_on_me;
    fastset memory_instrs_depend_on_me;

    std::array<uint32_t, NUM_INSTR_SOURCES> lq_index;
    std::array<uint32_t, NUM_INSTR_DESTINATIONS_SPARC> sq_index;
    std::array<uint32_t, NUM_INSTR_DESTINATIONS_SPARC> forwarding_index;

    ooo_model_instr() {
        instr_id = 0;
        ip = 0;
        fetch_producer = 0;
        producer_id = 0;
        translated_cycle = 0;
        fetched_cycle = 0;
        execute_begin_cycle = 0;
        retired_cycle = 0;
        event_cycle = 0;

        is_branch = 0;
        is_memory = 0;
        branch_taken = 0;
        branch_mispredicted = 0;
	branch_prediction_made = 0;
        translated = 0;
        data_translated = 0;
        is_producer = 0;
        is_consumer = 0;
        reg_RAW_producer = 0;
        fetched = 0;
        scheduled = 0;
        executed = 0;
        reg_ready = 0;
        mem_ready = 0;
        asid.fill(std::numeric_limits<uint8_t>::max());

	branch_type = NOT_BRANCH;
	branch_target = 0;

        instruction_pa = 0;
        data_pa = 0;
        virtual_address = 0;
        physical_address = 0;

        num_reg_ops = 0;
        num_mem_ops = 0;
        num_reg_dependent = 0;

        source_registers.fill(0);
        source_memory.fill(0);
        source_virtual_address.fill(0);
        source_added.fill(0);
        lq_index.fill(std::numeric_limits<uint32_t>::max());
        reg_RAW_checked.fill(0);

        destination_memory.fill(0);
        destination_registers.fill(0);
        destination_virtual_address.fill(0);
        destination_added.fill(0);
        sq_index.fill(std::numeric_limits<uint32_t>::max());
        forwarding_index.fill(0);

#if 0
        registers_instrs_depend_on_me.fill(0);
        memory_instrs_depend_on_me.fill(0);

        for (uint32_t i=0; i<ROB_SIZE; i++) {
            for (uint32_t j=0; j<NUM_INSTR_SOURCES; j++)
                registers_index_depend_on_me[i][j] = 0;
        }
#endif
    };

  void print_instr()
  {
    cout << "*** " << instr_id << " ***" << endl;
    cout << hex << "0x" << (uint64_t)ip << dec << endl;
    cout << (uint32_t)is_branch << " " << (uint32_t)branch_taken << endl;
    for(uint32_t i=0; i<NUM_INSTR_SOURCES; i++)
      {
	cout << (uint32_t)source_registers[i] << " ";
      }
    cout << endl;
    for(uint32_t i=0; i<NUM_INSTR_SOURCES; i++)
      {
	cout << hex << "0x" << (uint32_t)source_memory[i] << dec << " ";
      }
    cout << endl;
    for(uint32_t i=0; i<NUM_INSTR_DESTINATIONS; i++)
      {
	cout << (uint32_t)destination_registers[i] << " ";
      }
    cout << endl;
    for(uint32_t i=0; i<NUM_INSTR_DESTINATIONS; i++)
      {
        cout << hex << "0x" << (uint32_t)destination_memory[i] << dec << " ";
      }
    cout << endl;
    
    cout << endl;
  }
};

#endif
