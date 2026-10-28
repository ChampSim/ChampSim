#include "random.h"

champsim::modules::replacement::register_module<struct random> random_register("random");
random::random(champsim::modules::ModuleBuilder builder)
    : random(builder.get_parent<champsim::modules::cache_module>(), builder.get_parent<champsim::modules::cache_module>()->num_ways())
{
}

random::random(champsim::modules::cache_module* cache, long ways) : dist(0, ways - 1) {}

long random::find_victim(champsim::origin origin, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                         champsim::address full_addr, access_type type)
{
  return dist(rng);
}
