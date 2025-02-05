#ifndef BRANCH_GLOBAL_H
#define BRANCH_GLOBAL_H

#include <array>
#include <bitset>
#include <iostream>
// why do we include these includes?
#include "address.h"
#include "modules.h" 
#include "msl/fwcounter.h"

// TODO: edit this struct to only have the global predictor
struct global : champsim::modules::branch_predictor {
    
    // based on the values used by the Alpha 21264 microprocessor
    
    // local history predictor
    
    // path history length
    static constexpr std::size_t GLOBAL_HISTORY_LENGTH = 12;

    std::bitset<GLOBAL_HISTORY_LENGTH> path_history_register;

    // global history predictor
    static constexpr std::size_t GLOBAL_PREDICTION_TABLE_SIZE = 4096;
    static constexpr std::size_t GLOBAL_PREDICTION_COUNTER_BITS = 2;

    std::array<champsim::msl::fwcounter<GLOBAL_PREDICTION_COUNTER_BITS>, GLOBAL_PREDICTION_TABLE_SIZE> global_prediction_table;


public:  
    using branch_predictor::branch_predictor;

    bool predict_branch(champsim::address ip);
    void last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type); 
    
};

#endif
