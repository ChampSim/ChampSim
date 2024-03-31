#ifndef STATE_MODEL_INCLUSIVE_H
#define STATE_MODEL_INCLUSIVE_H

#include <vector>

#include "access_type.h"
#include "cache.h"
#include "modules.h"


struct inclusive : champsim::modules::state_model {

  enum class STATE { MODIFIED, INVALID };
  long NUM_SET, NUM_WAY;
  std::vector<STATE> cache_state;
  uint64_t cycle = 0;

  explicit inclusive(CACHE* cache);
  inclusive(CACHE* cache, long sets, long ways);
  CACHE::state_response_type handle_request(champsim::address address, long set, access_type type, bool hit, uint32_t cpu);
  CACHE::state_response_type handle_response(champsim::address address, long set, access_type type, uint32_t cpu);
  void initialize_state_model();
  void final_stats();

};

#endif
