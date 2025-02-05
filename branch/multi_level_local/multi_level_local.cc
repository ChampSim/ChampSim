#include "multi_level_local.h"

bool multi_level_local::predict_branch(champsim::address ip)
{
       

    // do the selector logic
    auto value = selector_prediction_table[path_history_register.to_ullong()];

    if (value.value() > (value.maximum / 2))
    {
        // do global predictor

        auto global_prediction = global_prediction_table[path_history_register.to_ullong()];
        return global_prediction.value() > (global_prediction.maximum / 2);
    }
    else
    {
        // do the local predictor
        
        std::bitset<LOCAL_HISTORY_LENGTH> local_history = local_history_table[hash(ip)];
        auto local_prediction = local_prediction_table[local_history.to_ullong()];
        return local_prediction.value() > (local_prediction.maximum / 2); 
    }

    return false;
}

void multi_level_local::last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type)
{
    // std::cout<< "CREATING GLOBAL AND LOCAL CORRECT BOOLEANS" << std::endl;
    // create global correct and local correct booleans
    bool is_global_correct = false;
    bool is_local_correct = false;

    // std::cout << "CHECKING AND UPDATING GLOBAL PREDICTOR" << std::endl;
    // check and update global predictor
    auto global_prediction = global_prediction_table[path_history_register.to_ullong()];
    is_global_correct = ( global_prediction.value() > (global_prediction.maximum / 2) ) == taken;

    global_prediction_table[path_history_register.to_ullong()] += (taken) ? 1 : -1;

    // std::cout << "CHECKING AND UPDATING LOCAL PREDICTOR" << std::endl;
    // check and update local predictor
    auto hashed_ip = hash(ip);
    std::bitset<LOCAL_HISTORY_LENGTH> local_history = local_history_table[hashed_ip];
    auto local_prediction = local_prediction_table[local_history.to_ullong()];
    is_local_correct = ( local_prediction.value() > (local_prediction.maximum / 2) ) == taken;
    
    local_prediction_table[local_history.to_ullong()] += taken ?  1 : -1;
    local_history_table[hashed_ip] <<= 1;
    local_history_table[hashed_ip] |= taken ? 1 : 0;

    // std::cout << "CHECKING AND UPDATING SELECTOR" << std::endl;
    // update selector 
    if (is_local_correct && !is_global_correct) selector_prediction_table[path_history_register.to_ulong()] -= 1;
    else if (!is_local_correct && is_global_correct) selector_prediction_table[path_history_register.to_ulong()] += 1;

    // update the path history register
    path_history_register <<= 1; 
    path_history_register |= taken ? 1 : 0;
}
