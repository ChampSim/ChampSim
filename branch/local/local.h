#ifndef BRANCH_LOCAL_H
#define BRANCH_LOCAL_H

#include <array>
#include <bitset>
#include <iostream>
// why do we include these includes?
#include "address.h"
#include "modules.h" 
#include "msl/fwcounter.h"

// TODO: finish struct
struct local : champsim::modules::branch_predictor {
    
    // based on the values used by the Alpha 21264 microprocessor

    [[nodiscard]] static constexpr auto hash(champsim::address ip) { return (ip.to<unsigned long>() >> 4) % PRIME; }
    
    // local history predictor
    static constexpr std::size_t LOCAL_HISTORY_LENGTH = 10;
    static constexpr std::size_t LOCAL_HISTORY_TABLE_SIZE = 1024;
    static constexpr std::size_t PRIME = 997;

    static constexpr std::size_t LOCAL_PREDICTION_TABLE_SIZE = 1 << LOCAL_HISTORY_LENGTH;
    static constexpr std::size_t LOCAL_PREDICTION_COUNTER_BITS = 3;

    std::array<std::bitset<LOCAL_HISTORY_LENGTH>, LOCAL_HISTORY_TABLE_SIZE> local_history_table;
    std::array<champsim::msl::fwcounter<LOCAL_PREDICTION_COUNTER_BITS>, LOCAL_PREDICTION_TABLE_SIZE> local_prediction_table;

public:  
    using branch_predictor::branch_predictor;

    bool predict_branch(champsim::address ip);
    void last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type); 
};

#endif
