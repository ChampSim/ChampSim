#include "bytecode_module.h"

void BYTECODE_MODULE::initialize(uint32_t cpu) {
    this->cpu = cpu;
}

uint64_t BYTECODE_MODULE::predict_branching(int opcode, int oparg, uint64_t current_bpc) 
{
    // handle branch prediction for all instructions as at this point we do not know if the instruction is a branch
    auto predicted_branch_target = btb_prediction(opcode, oparg);
    last_branch_opcode = opcode;
    last_branch_oparg = oparg;
    last_bpc = current_bpc;
    if (predicted_branch_target == 0) {
        last_prediction = 0;
        return current_bpc + BYTECODE_SIZE * BYTECODE_FETCH_TIME;
    }
    last_prediction = current_bpc + predicted_branch_target;
    return current_bpc + predicted_branch_target;
}

void BYTECODE_MODULE::updateBranching(uint64_t correct_target)
{
    if (last_prediction != 0) {
        if (last_prediction == correct_target) {
            stats.strongly_correct++;
        } else if ((last_prediction >> LOG2_BB_BUFFER_SIZE) << LOG2_BB_BUFFER_SIZE == (correct_target >> LOG2_BB_BUFFER_SIZE) << LOG2_BB_BUFFER_SIZE) {
            stats.weakly_correct++;
        } else {
            stats.wrong++;
        };
    }

    update_btb(last_branch_opcode, last_branch_oparg, correct_target - last_bpc);
}
