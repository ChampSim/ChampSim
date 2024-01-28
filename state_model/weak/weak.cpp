#include "weak.h"

weak::weak(CACHE* cache) : weak(cache, cache->NUM_SET, cache->NUM_WAY) {}

weak::weak(CACHE* cache, long sets, long ways) : state_model(cache), NUM_SET(sets), NUM_WAY(ways), cache_state(static_cast<std::size_t>(sets * ways), 0) {}

weak::weak(CACHE* cache, long sets, long ways) : 
  state_model(cache), NUM_SET(sets), NUM_WAY(ways), 
  current_state(static_cast<std::size_t>(sets * ways), 0) {}
