#include "weak.h"

weak::weak(CACHE* cache) : weak(cache, cache->NUM_SET, cache->NUM_WAY) {}

weak::weak(CACHE* cache, long sets, long ways) : state_model(cache), NUM_SET(sets), NUM_WAY(ways), cache_state(static_cast<std::size_t>(sets * ways), STATE::INVALID) {}

void weak::initialize_state_model(){}
void weak::final_stats(){}

//Defaults to the cache model's already existing operation
//Must rework the cache if this is to be strongly defined
CACHE::state_response_type weak::handle_request(champsim::address address, long set, access_type type, bool hit, uint32_t cpu){
  //invalidate_entry(req);

  CACHE::state_response_type state_response;
  state_response.send_lower_level = access_type::NONE;
  state_response.send_upper_level = access_type::NONE;
  state_response.handle_at_this_level = access_type::NONE;
  state_response.state_model_stall = false;
  state_response.address = address;

  return state_response;
}

CACHE::state_response_type weak::handle_response(champsim::address address, long set, access_type type, uint32_t cpu){
  return CACHE::state_response_type();
}
