#include "lru.h"

#include <algorithm>
#include <cassert>

champsim::modules::replacement::register_module<lru> lru_register("lru");

lru::lru(champsim::modules::ModuleBuilder builder)
    : lru(builder.get_parent<champsim::modules::cache_module>(), builder.get_parent<champsim::modules::cache_module>()->num_sets(),
          builder.get_parent<champsim::modules::cache_module>()->num_ways())
{
}

lru::lru(champsim::modules::cache_module* cache, long sets, long ways) : NUM_WAY(ways), last_used_cycles(static_cast<std::size_t>(sets * ways), 0) {}

long lru::find_victim(champsim::origin origin, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                      champsim::address full_addr, access_type type)
{
  auto begin = std::next(std::begin(last_used_cycles), set * NUM_WAY);
  auto end = std::next(begin, NUM_WAY);

  // Find the way whose last use cycle is most distant
  auto victim = std::min_element(begin, end);
  assert(begin <= victim);
  assert(victim < end);
  return std::distance(begin, victim);
}

void lru::replacement_cache_fill(champsim::origin origin, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                 access_type type)
{
  // Mark the way as being used on the current cycle
  last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;
}

void lru::update_replacement_state(champsim::origin origin, long set, long way, champsim::address full_addr, champsim::address ip,
                                   champsim::address victim_addr, access_type type, bool hit)
{
  // Mark the way as being used on the current cycle
  if (hit && access_type{type} != access_type::WRITE) // Skip this for writeback hits
    last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;
}
