#ifdef CHAMPSIM_MODULE
#define SET_ASIDE_CHAMPSIM_MODULE
#undef CHAMPSIM_MODULE
#endif

#ifndef BYTECODE_MODULE_H
#define BYTECODE_MODULE_H

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>

#include "cache.h"
#include "champsim.h"
#include "deadlock.h"
#include "instruction.h"
#include "bytecode_buffer.h"
#include "util/span.h"
#include <fmt/chrono.h>
#include <fmt/core.h>
#include <fmt/ranges.h>



struct BYTCODE_MODULE_STATS {
    uint64_t strongly_correct = 0;
    uint64_t weakly_correct = 0;
    uint64_t wrong = 0;

    double BTB_PERCENTAGE = 0;
};

class BYTECODE_MODULE {
    uint32_t cpu = 0;
    
    int last_branch_opcode; 
    int last_branch_oparg; 

    uint64_t last_bpc, last_prediction;

    bool make_btb_prediction(int opcode, int oparg);
    int64_t btb_prediction(int opcode, int oparg);
    void update_btb(int opcode, int oparg, int64_t correct_jump);
    
    public: 
        BYTECODE_BUFFER bb_buffer;
        BYTCODE_MODULE_STATS stats;
        void printBTBs();

        void initialize(uint32_t cpu);
        uint64_t predict_branching(int opcode, int oparg,  uint64_t current_bpc);
        void updateBranching(uint64_t correct_target);
};


#endif