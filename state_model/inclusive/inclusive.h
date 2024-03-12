#ifndef STATE_MODEL_INCLUSIVE_H
#define STATE_MODEL_INCLUSIVE_H

#include <vector>

#include "cache.h"
#include "modules.h"
#include "access_type.h"


struct inclusive : champsim::modules::state_model {

  enum class STATE { MODIFIED, INVALID };
  long NUM_SET, NUM_WAY;
  std::vector<STATE> cache_state;
  uint64_t cycle = 0;

  explicit inclusive(CACHE* cache);
  inclusive(CACHE* cache, long sets, long ways);
  bool handle_pkt(champsim::address address, champsim::address ip, access_type type, uint32_t cpu);
  bool handle_response(champsim::channel::response_type resp);
  void initialize_state_model();
  void final_stats();

};

#endif
