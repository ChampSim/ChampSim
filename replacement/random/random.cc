#include "random.h"

random::random(CACHE* cache) : random(cache, cache->NUM_WAY) {}

random::random(CACHE* cache, long ways) : replacement(cache), dist(0, ways - 1) {}

long random::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const CACHE::BLOCK* current_set, uint64_t ip, uint64_t full_addr,
                         access_type type)
{
  return dist(rng);
}
