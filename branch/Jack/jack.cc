#include "ooo_cpu.h"
#include <random> 
#include <map>

namespace
{
bool Last_bit = 1;
} // namespace

void O3_CPU::initialize_branch_predictor() {}

uint8_t O3_CPU::predict_branch(uint64_t ip)
{
srand(unsigned(time(0)));

return 1;
}

void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{

}
