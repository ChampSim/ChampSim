#include <cstdint>
#include <map>
#include <vector>
#include <cassert>
#include <math.h>

// /inc/cache.sh
#include "cache.h"
// /inc/channel.h
#include "channel.h"



/* begin: hawkeye predictor */

// implements the CRC32 algorithm
// used in HawkeyePredictor make a hash for input addresses
// reference: https://gist.github.com/timepp/1f678e200d9e0f2a043a9ec6b3690635
uint64_t CRC32(uint64_t x) {
  const unsigned long long P = 0xedb88320ULL;
  unsigned long long output = x;
  for (uint8_t i=0; i<32; i++) {
    if ((output & 1) == 1) {
      output = P ^ (output >> 1);
    } else {
      output = output >> 1;
    }
  }
  return output;
}

// use 11-bit hashes and 5-bit counters, as seen in 2017 paper
#define HAWKEYE_PREDICTOR_HASH_LEN 11
#define HAWKEYE_PREDICTOR_COUNTER_LEN 5
// (1 << 5) - 1 == 31
#define HAWKEYE_PREDICTOR_COUNTER_MAX 31

// predict whether a given PC is cache-friendly/averse
class HawkeyePredictor
{
// hashtable: pc hash => pc value
std::map<uint64_t, short unsigned int> ht;

public:
  void increment(uint64_t pc) {
    uint64_t key = hash(pc);

    // if not found, first set to half the maximum value (neutral)
    if (ht.find(key) == ht.end()) {
      ht[key] = (1 + HAWKEYE_PREDICTOR_COUNTER_MAX) / 2;
    }

    // increment value, no exceeding maximum
    if (ht[key] < HAWKEYE_PREDICTOR_COUNTER_MAX) {
      ht[key] = ht[key] + 1;
    }
  }

  void decrement(uint64_t pc) {
    uint64_t key = hash(pc);

    // if not found, first set to half the maximum value (neutral)
    if (ht.find(key) == ht.end()) {
      ht[key] = (1 + HAWKEYE_PREDICTOR_COUNTER_MAX) / 2;
    }

    // decrement value, no lower than 0
    if (ht[key] > 0) {
      ht[key] = ht[key] - 1;
    }
  }

  bool predict(uint64_t pc) {
    uint64_t key = hash(pc);

    // if not found or greater than half the max, return true
    if (ht.find(key) == ht.end()) {
      return true;
    }
    if (ht[key] >= (HAWKEYE_PREDICTOR_COUNTER_MAX+1)/2) {
      return true;
    }
    return false;
  }

private:
  uint64_t hash(uint64_t pc) {
    uint64_t hash_ = CRC32(pc) % (1 << HAWKEYE_PREDICTOR_HASH_LEN);
    return hash_;
  }
};

/* end: hawkeye predictor */


/* begin: OPTgen */
// 8 times of associativity, as seen in the papers
#define OV_LEN 128

struct OPTgen
{
  std::vector<unsigned int> ov; // occupancy vector
  uint64_t capacity; // cache capacity

  void init(uint64_t capacity_) {
    capacity = capacity_;
    ov.resize(OV_LEN, 0);
  }

  void add(uint64_t curr_time) {
    ov[curr_time] = 0;
  }

  // decide whether the given line
  // whose usage interval is [last_time, curr_time)
  // should be cached under the OPT policy
  bool decide(uint64_t curr_time, uint64_t last_time) {
    bool should_cache = true;

    // every element of ov in [last_time, curr_time)
    // should be smaller than cache capacity
    // if the given line should be cached
    unsigned int i = last_time;
    while (i != curr_time) {
      if (ov[i] >= capacity) {
        should_cache = false;
        break;
      }
      i = (i+1) % ov.size();
    }

    if (should_cache) {
      // increment ov[last_time: curr_time] by 1
      // indicating the given line is cached
      i = last_time;
      while (i != curr_time) {
        ov[i] = ov[i] + 1;
        i = (i+1) % ov.size();
      }
      assert(i == curr_time);
    }

    return should_cache;
  }
};

/* end: OPTgen */


/* begin: MyAddress */

// Sampler entry for access of a specific data address. 
// In the actual sampler, the address will be denoted by the key of the map in which the sampler entry will reside in 
struct MyAddress
{
    uint32_t last_time; // Last time this entry is accessed 
    uint64_t pc; // The PC that accessed this entry
    uint32_t age; // Age bits that to determine which entry to clear when the sampler is full

    void init(unsigned int curr_quanta)
    {
        last_time = 0;
        pc = 0;
        age = 0;
    }

    // 
    void update(unsigned int curr_time, uint64_t pc_)
    {
        last_time = curr_time;
        pc = pc_;
    }

};

/* end:  MyAddress*/




/* begin: hawkeye main */
// use 3-bit RRIP counters
#define RRIP_MAX 7


// champsim default configure
// 2048 blocks, 16 ways
#define NUM_SETS 2048
#define NUM_WAYS 16
uint8_t rrip[NUM_SETS][NUM_WAYS];

// track the last access time for each set
#define TIMER_MAX 1024
uint64_t timers[NUM_SETS];

// PCs in the cache
uint64_t pc_matrix[NUM_SETS][NUM_WAYS];


// predictor
HawkeyePredictor* predictor;


// optgen
// one independent OPTgen for each set
OPTgen optgens[NUM_SETS];


// determine whether the given set number is in the sample
// set < 2048 == 1 << 11, so set has at most 11 bits
// fix 5 bits, we have 6 free bits, so total 64 are sampled
bool is_sample(uint32_t set) {
  unsigned long mask = 0b10101100100;
  return (set & mask) == mask;
}
// bool is_sample(uint32_t set) {
//   return ((set >> 5) & 63L) == (set & 63L);
// }

#define SAMPLED_CACHE_SIZE 2800
#define SAMPLER_WAYS 8
#define SAMPLER_SETS SAMPLED_CACHE_SIZE / SAMPLER_WAYS


// sampler
std::vector<std::map<uint64_t, MyAddress> > addr_history;

// Helper function for removing sampler entry
void remove_old_sampler_entry(unsigned int sampler_set)
{
  // index for the entry to remove
  uint64_t remove_key = 0;
  
  // Iterate through all entries to find the oldest one and remove
  for (auto it = addr_history[sampler_set].begin(); it != addr_history[sampler_set].end(); it++) {
    if ((it->second).age == (SAMPLER_WAYS - 1)) {
      remove_key = it->first;
      break;
    }
  }

  addr_history[sampler_set].erase(remove_key);
}

// Helper function for aging sampler entries for determining which to evict when full with lru
void age_sampler_entries(unsigned int sampler_set, unsigned int curr_age)
{
  for (auto it = addr_history[sampler_set].begin(); it != addr_history[sampler_set].end(); it++) {
    // Age all the entries that are later than a specific curr_age.
    // Curr_age will be set to set to max value when a new entry is added so when full the highest will be exactly SAMPLER_WAYS - 1.
    // Otherwise it will be called when accessing a specfic entry, with curr_age being the age of that entry, so that the entres younger than
    // that can still be aged without messing with max value or the right order 
    if ((it->second).age < curr_age) {
      (it->second).age++;
      assert((it->second).age < SAMPLER_WAYS);
    }
  }
}



// CACHE implemenation
// state initialization
void CACHE::initialize_replacement() {
  for (int i=0; i<NUM_SETS; i++) {
    for (int j=0; j<NUM_WAYS; j++) {
      // set all RRIP values to maximum
      rrip[i][j] = RRIP_MAX;

      pc_matrix[i][j] = 0;
    }

    timers[i] = 0;
    optgens[i].init(NUM_WAYS-2);
  }
  
  addr_history.resize(SAMPLER_SETS);
  for (int i=0; i<SAMPLER_SETS; i++) {
    addr_history[i].clear();
  }
  
  predictor = new HawkeyePredictor();
}

// find the way index to evict
// return NUM_WAYS to indicate no eviction
uint32_t CACHE::find_victim(uint32_t triggering_cpu, uint64_t instr_id, uint32_t set, const BLOCK* current_set, uint64_t PC, uint64_t paddr, uint32_t type) {
  // logic: find the most 'cache-averse' line to evict
  // RRIP == 7 indicates cache-averse
  // RRIP < 7 indicates cache-friendly
  // first, try to find a first cache-averse line and evict it
  // if no cache-averse, evict the oldest cache-friendly with LRU
  // (largest RRIP)

	// 1. try to find a cache-averse line (RRIP==7)
  for (uint32_t i=0; i<NUM_WAYS; i++) {
    if (rrip[set][i] == RRIP_MAX) {
    	return i;
    }
  }
  
  // 2. cannot find a cache-averse line, evict largest RRIP
  uint8_t rrip_max = -1; // hold current maximum of rrip
  int32_t victim = -1;
  for (uint32_t i=0; i<NUM_WAYS; i++) {
    if (rrip[set][i] > rrip_max) {
      rrip_max = rrip[set][i];
      victim = i;
    }
  }

  assert(victim != -1);
  // train the predictor negatively for a PC evicted by LRU
	if (is_sample(set)) {
    predictor->decrement(pc_matrix[set][victim]);
  }
  return victim;
}




void CACHE::update_replacement_state(uint32_t triggering_cpu, uint32_t set, uint32_t way, uint64_t paddr, uint64_t PC, uint64_t victim_addr, uint32_t type, uint8_t hit) {
	// do not update replacement state for writebacks
  // check drrip.cc for the usage of access_type
  if (access_type{type} == access_type::WRITE) {
    return;
  }
  
  if (is_sample(set)) {
  	paddr = (paddr >> 6) << 6; // wipe the least significant 6 bits
    uint64_t curr_time = timers[set] % OV_LEN;

    // calculate set and tag values in the sampled cache
    uint32_t sampler_set = (paddr >> 6) % SAMPLER_SETS;
    uint64_t sampler_tag = CRC32(paddr >> 12) % 256;

    if ((
      addr_history[sampler_set].find(sampler_tag)
      != addr_history[sampler_set].end()
    )) {
      unsigned int timer = timers[set];
      if (timer < addr_history[sampler_set][sampler_tag].last_time) {
        timer = timer + TIMER_MAX;
      }
      bool wrap = (timer - addr_history[sampler_set][sampler_tag].last_time) > OV_LEN;
      uint64_t last_time = addr_history[sampler_set][sampler_tag].last_time % OV_LEN;
      if (!wrap && optgens[set].decide(curr_time, last_time)) {
        predictor->increment(addr_history[sampler_set][sampler_tag].pc);
      } else {
        predictor->decrement(addr_history[sampler_set][sampler_tag].pc);
      }

      optgens[set].add(curr_time);
      age_sampler_entries(sampler_set, addr_history[sampler_set][sampler_tag].age);
    } else {
      if (addr_history[sampler_set].size() == SAMPLER_WAYS) {
        remove_old_sampler_entry(sampler_set);
      }
      addr_history[sampler_set][sampler_tag].init(curr_time);
      optgens[set].add(curr_time);
      age_sampler_entries(sampler_set, SAMPLER_WAYS-1);
    }

    addr_history[sampler_set][sampler_tag].update(timers[set], PC);
    addr_history[sampler_set][sampler_tag].age = 0;
    timers[set] = (timers[set] + 1) % TIMER_MAX;
  }

  bool cache_friendly = predictor->predict(PC);
  if (!cache_friendly) {
    // cache-averse, set RRIP to maximum
    rrip[set][way] = RRIP_MAX;
  } else {
    // cache-friendly, age all other cache-friendly lines
    // but only if no one will become cache-averse (RRIP < maximum)
    bool should_age = true;
    for (uint32_t i=0; i<NUM_WAYS; i++) {
      if (rrip[set][i] == RRIP_MAX-1) {
        should_age = false;
      }
    }
    
    if (should_age) {
      for (uint32_t i=0; i<NUM_WAYS; i++) {
        if (rrip[set][i] < RRIP_MAX-1) {
          rrip[set][i] = rrip[set][i] + 1;
        }
      }
    }
  }

}

void CACHE::replacement_final_stats() {}
/* end: hawkeye main */
