#ifndef STATE_MODEL_WEAK_H
#define STATE_MODEL_WEAK_H

#include <vector>

#include "access_type.h"
#include "cache.h"
#include "modules.h"

struct weak : champsim::modules::state_model {
  enum class STATE { MODIFIED, WAITING_ACK, WAITING_FILL, INVALID };
  long NUM_SET, NUM_WAY;
  std::vector<STATE> cache_state;
  uint64_t cycle = 0;

  explicit weak(CACHE* cache);
  weak(CACHE* cache, long sets, long ways);
  CACHE::state_response_type handle_request(champsim::address address, long set, access_type type, bool hit, uint32_t cpu);
  CACHE::state_response_type handle_response(champsim::address address, long set, access_type type, uint32_t cpu);
  void initialize_state_model();
  void final_stats();
};

#endif
