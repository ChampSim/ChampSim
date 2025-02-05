#include "multi_level_local_32k.h"

bool multi_level_local_32k::predict_branch(champsim::address ip)
{
    // do the selector logic
    auto selector_value = selector_prediction_table[path_history_register.to_ullong()];

    if (selector_value.value() > (selector_value.maximum / 2))
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

void multi_level_local_32k::last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type)
{
    // instantiate variables and references 
    auto path_history_index = path_history_register.to_ullong();
    auto hashed_ip = hash(ip);
    auto &global_prediction = global_prediction_table[path_history_index];
    auto &local_history = local_history_table[hashed_ip];
    auto &local_prediction = local_prediction_table[local_history.to_ullong()];
    auto &selector_value = selector_prediction_table[path_history_index];

    // check global correct and local correct booleans
    bool is_global_correct = ( global_prediction.value() > (global_prediction.maximum / 2) ) == taken;
    bool is_local_correct = ( local_prediction.value() > (local_prediction.maximum / 2) ) == taken;

    // update predictors
    global_prediction += taken ? 1 : -1;
    local_prediction += taken ? 1 : -1;

    // update local history
    local_history <<= 1;
    local_history |= taken ? 1 : 0;

    // update selector
    if (is_local_correct != is_global_correct)
    {
        selector_value += is_global_correct ? 1 : -1;       
    }

    // update the path history register
    path_history_register <<= 1; 
    path_history_register |= taken ? 1 : 0;
}
