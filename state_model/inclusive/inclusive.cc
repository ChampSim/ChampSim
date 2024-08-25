#include "inclusive.h"

inclusive::inclusive(CACHE* cache) : inclusive(cache, cache->NUM_SET, cache->NUM_WAY) {}

inclusive::inclusive(CACHE* cache, long sets, long ways) 
  : state_model(cache), 
  NUM_SET(sets), 
  NUM_WAY(ways), 
  cache_state(static_cast<std::size_t>(sets * ways), STATE::INVALID) {}

void inclusive::initialize_state_model(){ 
}

void inclusive::final_stats(){

}

CACHE::state_response_type inclusive::handle_request(champsim::address address, long set, access_type type, bool hit, uint32_t cpu){

  CACHE::state_response_type state_response;
  state_response.address = address;

  //Inclusive cache invalidates upper level caches when its cache line is evicted
  if (type == access_type::EVICT) {
    state_response.send_lower_level = access_type::INVALIDATE;
    state_response.send_upper_level = access_type::NONE;
    state_response.handle_at_this_level = access_type::NONE; //This needs to change to a self invalidate after updating cache code
    state_response.state_model_stall = false;

  //If this level of the cache is receiving an invalidation request,
  //send it to the upper level to maintain inclusioin 
  } else if ( type == access_type::INVALIDATE) {
    state_response.send_lower_level = access_type::INVALIDATE;
    state_response.send_upper_level = access_type::NONE;
    state_response.handle_at_this_level = access_type::INVALIDATE; //This needs to change to a self invalidate after updating cache code
    state_response.state_model_stall = false;
  } else {
    state_response.send_lower_level = access_type::NONE;
    state_response.send_upper_level = access_type::NONE;
    state_response.handle_at_this_level = access_type::NONE;
    state_response.state_model_stall = false; 
  }


  return state_response;
}

CACHE::state_response_type inclusive::handle_response(champsim::address address, long set, access_type type, uint32_t cpu){
  return CACHE::state_response_type();
}
