#include "inclusive.h"

inclusive::inclusive(CACHE* cache) : inclusive(cache, cache->NUM_SET, cache->NUM_WAY) {}

inclusive::inclusive(CACHE* cache, long sets, long ways) 
  : state_model(cache), 
  NUM_SET(sets), 
  NUM_WAY(ways), 
  cache_state(static_cast<std::size_t>(sets * ways), STATE::INVALID) {}

void inclusive::initialize_state_model(){ printf("[%s] INITIALIZING INCLUSIVE STATE MODEL\n", __FUNCTION__); }
void inclusive::final_stats(){
  printf("WE MADE IT TO THE FINAL STATS\n"); 

}


bool inclusive::handle_pkt(champsim::address address, champsim::address v_address, access_type type, uint32_t cpu)
{
  printf("HERE!!!!n");
  return true;
}

bool inclusive::handle_response(champsim::channel::response_type resp){
  return true;

}


