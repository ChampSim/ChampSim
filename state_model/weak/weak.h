#ifndef STATE_MODEL_WEAK_H
#define STATE_MODEL_WEAK_H

#include <vector>

#include "cache.h"
#include "modules.h"
#include "channel.h"

using request_type = typename champsim::channel::request_type;
using response_type = typename champsim::channel::response_type;

//Define possible states here
enum class STATE { MODIFIED, INVALID };

struct weak : champsim::modules::state_model {
  long NUM_SET, NUM_WAY;
  std::vector<STATE> current_state;
  uint64_t cycle = 0;

  explicit weak(CACHE* cache);
  weak(CACHE* cache, long sets, long ways);

  bool state_model_handle_request(request_type req);
  bool state_model_handle_response(response_type resp);

};

#endif
