#ifndef STATE_MODEL_WEAK_H
#define STATE_MODEL_WEAK_H

#include <vector>

#include "cache.h"
#include "modules.h"

enum class STATE { MODIFIED, INVALID };

struct weak : champsim::modules::state_model {
  long NUM_SET, NUM_WAY;
  std::vector<STATE> cache_state;
  uint64_t cycle = 0;

  explicit weak(CACHE* cache);
  weak(CACHE* cache, long sets, long ways);

};

#endif
