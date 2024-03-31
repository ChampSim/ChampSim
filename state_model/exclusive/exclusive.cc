#include "exclusive.h"
#include "cache.h"

exclusive::exclusive(CACHE* cache) : exclusive(cache, cache->NUM_SET, cache->NUM_WAY) {}

exclusive::exclusive(CACHE* cache, long sets, long ways) : state_model(cache), NUM_SET(sets), NUM_WAY(ways), cache_state(static_cast<std::size_t>(sets * ways), STATE::INVALID) {
}

CACHE::state_response_type exclusive::handle_request(champsim::address address, long set, access_type type, bool hit, uint32_t cpu)
{
  CACHE::state_response_type state_response;

  if(hit){
    if (type == access_type::LOAD || type == access_type::RFO || type == access_type::WRITE) {
      state_response.send_lower_level = access_type::NONE;
      state_response.send_upper_level = access_type::NONE;
      state_response.handle_at_this_level = access_type::INVALIDATE;
      state_response.state_model_stall = false; 
    }
  } else {
    state_response.send_lower_level = access_type::NONE;
    state_response.send_upper_level = access_type::NONE;
    state_response.handle_at_this_level = access_type::NONE;
    state_response.state_model_stall = false; 
  }

  return state_response;
}

CACHE::state_response_type exclusive::handle_response(champsim::address address, long set, access_type type, uint32_t cpu)
{
  return CACHE::state_response_type();
}
