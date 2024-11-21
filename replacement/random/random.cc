#include "random.h"

champsim::modules::replacement::register_module<struct random> random_register("random");


void random::initialize_replacement() {
    dist = std::uniform_int_distribution<long>(0,intern_->NUM_WAY - 1);
}

long random::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip, champsim::address full_addr,
                         access_type type)
{
  return dist(rng);
}
