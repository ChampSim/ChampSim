#include "global.h"

// TODO: I THINK that the seg fault is in the path history register
// I think that because it is not initialized, the PHR is seg faulting
// the thing is tho that branches might be gettign predicted before the seg fault

bool global::predict_branch(champsim::address ip)
{
       

    // do the selector logic
    /*
    std::cout << "SELECTOR VALUE " << value.value() << std::endl;

    std::cout << "selector_prediction_table size: " << selector_prediction_table.size() << std::endl;
    std::cout << "global_prediction_table size: " << global_prediction_table.size() << std::endl;
    std::cout << "local_history_table size: " << local_history_table.size() << std::endl;
    */

    // do global predictor
    // std::cout << "GLOBAL PREDICTION" << std::endl;
    // std::cout << "path_history_register: " << path_history_register.to_ullong() << std::endl;

    auto global_prediction = global_prediction_table[path_history_register.to_ullong()];
    return global_prediction.value() > (global_prediction.maximum / 2);
}

void global::last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type)
{
    // std::cout<< "CREATING GLOBAL AND LOCAL CORRECT BOOLEANS" << std::endl;
    // create global correct and local correct booleans
    // bool is_global_correct = false;

    // std::cout << "CHECKING AND UPDATING GLOBAL PREDICTOR" << std::endl;
    // check and update global predictor
    // auto global_prediction = global_prediction_table[path_history_register.to_ullong()];
    // is_global_correct = ( global_prediction.value() > (global_prediction.maximum / 2) ) == taken;

    global_prediction_table[path_history_register.to_ullong()] += (taken) ? 1 : -1;

    // std::cout << "CHECKING AND UPDATING LOCAL PREDICTOR" << std::endl;
    // check and update local predictor

    // update the path history register
    std::cout << "UPDATING PATH HISTORY REGISTER" << std::endl;
    std::cout << "OLD PATH HISTORY REGISTER: " << path_history_register << " | integer format: " <<  path_history_register.to_ullong() << std::endl;
    path_history_register <<= 1; 
    path_history_register |= taken ? 1 : 0;
    std::cout << "NEW PATH HISTORY REGISTER: " << path_history_register << " | integer format: " <<  path_history_register.to_ullong() << std::endl;
}
