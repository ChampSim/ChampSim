#ifndef STATE_MODEL_WEAK_H
#define STATE_MODEL_WEAK_H

#include <vector>

#include "access_type.h"
#include "cache.h"
#include "modules.h"

struct exclusive : champsim::modules::state_model {

  enum class STATE { VALID, INVALID};

  long NUM_SET, NUM_WAY;
  std::vector<STATE> cache_state;
  uint64_t cycle = 0;

  std::map<champsim::address, STATE> complete_cache_state;

  explicit exclusive(CACHE* cache);
  exclusive(CACHE* cache, long sets, long ways);
  void initialize_state_model();
  CACHE::state_response_type handle_request(champsim::address address, long set, access_type type, bool hit, uint32_t cpu);
  CACHE::state_response_type handle_response(champsim::address address, long set, access_type type, uint32_t cpu);
  long get_state(champsim::address address);
  long update_state(champsim::address address);

};

#endif
