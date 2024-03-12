#include "weak.h"

weak::weak(CACHE* cache) : weak(cache, cache->NUM_SET, cache->NUM_WAY) {}

weak::weak(CACHE* cache, long sets, long ways) : state_model(cache), NUM_SET(sets), NUM_WAY(ways), cache_state(static_cast<std::size_t>(sets * ways), STATE::INVALID) {}

void weak::impl_initialize_state_model()
void weak::final_stats(){}

bool weak::handle_pkt(champsim::address address, champsim::address ip, access_type type, uint32_t cpu){
  //invalidate_entry(req);
  return true;
}

bool weak::handle_response(champsim::channel::response_type resp){
  return true;

}
