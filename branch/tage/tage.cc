#include "tage.h"

#include <ooo_cpu.h>

Tage tage_predictor[NUM_CPUS];

void O3_CPU::initialize_branch_predictor() { tage_predictor[cpu].init(); }

uint8_t O3_CPU::predict_branch(uint64_t ip) { return tage_predictor[cpu].predict(ip); }

void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type) { tage_predictor[cpu].update(ip, taken); }