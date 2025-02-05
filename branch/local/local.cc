#include "local.h"

bool local::predict_branch(champsim::address ip)
{
    // do the selector logic
    /*
    std::cout << "SELECTOR VALUE " << value.value() << std::endl;

    std::cout << "selector_prediction_table size: " << selector_prediction_table.size() << std::endl;
    std::cout << "global_prediction_table size: " << global_prediction_table.size() << std::endl;
    std::cout << "local_history_table size: " << local_history_table.size() << std::endl;
    */

    // do the local predictor
    // std::cout<< "LOCAL PREDICTION" << std::endl;
    // std::cout << "hashed_ip: " << hash(ip) << std::endl;
    // std::cout << "local_history " << std::bitset<LOCAL_HISTORY_LENGTH>(local_history_table[hash(ip)]) << std::endl;
    
    std::bitset<LOCAL_HISTORY_LENGTH> local_history = local_history_table[hash(ip)];
    auto local_prediction = local_prediction_table[local_history.to_ullong()];
    return local_prediction.value() > (local_prediction.maximum / 2); 
}

void local::last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type)
{
    // std::cout<< "CREATING LOCAL CORRECT BOOLEANS" << std::endl;
    // create global correct and local correct booleans

    // std::cout << "CHECKING AND UPDATING LOCAL PREDICTOR" << std::endl;
    // check and update local predictor
    auto hashed_ip = hash(ip);
    std::bitset<LOCAL_HISTORY_LENGTH> local_history = local_history_table[hashed_ip];
    // auto local_prediction = local_prediction_table[local_history.to_ullong()];
    // is_local_correct = ( local_prediction.value() > (local_prediction.maximum / 2) ) == taken;
    
    local_prediction_table[local_history.to_ullong()] += taken ?  1 : -1;
    local_history_table[hashed_ip] <<= 1;
    local_history_table[hashed_ip] |= taken ? 1 : 0;
}
