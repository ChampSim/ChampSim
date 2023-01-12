#ifndef SPP_H
#define SPP_H

#include <cstdint>
#include <iostream>
#include <vector>

namespace spp
{
// SPP functional knobs
constexpr bool LOOKAHEAD_ON = true;
constexpr bool FILTER_ON = true;
constexpr bool GHR_ON = true;
constexpr bool SPP_SANITY_CHECK = true;
constexpr bool SPP_DEBUG_PRINT = false;

// Signature table parameters
constexpr std::size_t ST_SET = 1;
constexpr std::size_t ST_WAY = 256;
constexpr unsigned ST_TAG_BIT = 16;
constexpr uint32_t ST_TAG_MASK = ((1 << ST_TAG_BIT) - 1);
constexpr unsigned SIG_SHIFT = 3;
constexpr unsigned SIG_BIT = 12;
constexpr uint32_t SIG_MASK = ((1 << SIG_BIT) - 1);
constexpr unsigned SIG_DELTA_BIT = 7;

// Pattern table parameters
constexpr std::size_t PT_SET = 512;
constexpr std::size_t PT_WAY = 4;
constexpr unsigned C_SIG_BIT = 4;
constexpr unsigned C_DELTA_BIT = 4;
constexpr uint32_t C_SIG_MAX = ((1 << C_SIG_BIT) - 1);
constexpr uint32_t C_DELTA_MAX = ((1 << C_DELTA_BIT) - 1);

// Prefetch filter parameters
constexpr unsigned QUOTIENT_BIT = 10;
constexpr unsigned REMAINDER_BIT = 6;
constexpr unsigned HASH_BIT = (QUOTIENT_BIT + REMAINDER_BIT + 1);
constexpr std::size_t FILTER_SET = (1 << QUOTIENT_BIT);
constexpr uint32_t FILL_THRESHOLD = 90;
constexpr uint32_t PF_THRESHOLD = 25;

// Global register parameters
constexpr unsigned GLOBAL_COUNTER_BIT = 10;
constexpr uint32_t GLOBAL_COUNTER_MAX = ((1 << GLOBAL_COUNTER_BIT) - 1);
constexpr std::size_t MAX_GHR_ENTRY = 8;

enum FILTER_REQUEST { SPP_L2C_PREFETCH, SPP_LLC_PREFETCH, L2C_DEMAND, L2C_EVICT }; // Request type for prefetch filter
uint64_t get_hash(uint64_t key);

class SIGNATURE_TABLE
{
public:
  bool valid[ST_SET][ST_WAY];
  uint32_t tag[ST_SET][ST_WAY], last_offset[ST_SET][ST_WAY], sig[ST_SET][ST_WAY], lru[ST_SET][ST_WAY];

  SIGNATURE_TABLE()
  {
    for (uint32_t set = 0; set < ST_SET; set++)
      for (uint32_t way = 0; way < ST_WAY; way++) {
        valid[set][way] = 0;
        tag[set][way] = 0;
        last_offset[set][way] = 0;
        sig[set][way] = 0;
        lru[set][way] = way;
      }
  };

  void read_and_update_sig(uint64_t page, uint32_t page_offset, uint32_t& last_sig, uint32_t& curr_sig, int32_t& delta);
};

class PATTERN_TABLE
{
public:
  int delta[PT_SET][PT_WAY];
  uint32_t c_delta[PT_SET][PT_WAY], c_sig[PT_SET];

  PATTERN_TABLE()
  {
    for (uint32_t set = 0; set < PT_SET; set++) {
      for (uint32_t way = 0; way < PT_WAY; way++) {
        delta[set][way] = 0;
        c_delta[set][way] = 0;
      }
      c_sig[set] = 0;
    }
  }

  void update_pattern(uint32_t last_sig, int curr_delta), read_pattern(uint32_t curr_sig, std::vector<int>&prefetch_delta, std::vector<uint32_t>&confidence_q,
                                                                       uint32_t&lookahead_way, uint32_t&lookahead_conf, uint32_t&pf_q_tail, uint32_t&depth);
};

class PREFETCH_FILTER
{
public:
  uint64_t remainder_tag[FILTER_SET];
  bool valid[FILTER_SET], // Consider this as "prefetched"
      useful[FILTER_SET]; // Consider this as "used"

  PREFETCH_FILTER()
  {
    for (uint32_t set = 0; set < FILTER_SET; set++) {
      remainder_tag[set] = 0;
      valid[set] = 0;
      useful[set] = 0;
    }
  }

  bool check(uint64_t pf_addr, FILTER_REQUEST filter_request);
};

class GLOBAL_REGISTER
{
public:
  // Global counters to calculate global prefetching accuracy
  uint32_t pf_useful, pf_issued;
  uint32_t global_accuracy; // Alpha value in Section III. Equation 3

  // Global History Register (GHR) entries
  uint8_t valid[MAX_GHR_ENTRY];
  uint32_t sig[MAX_GHR_ENTRY], confidence[MAX_GHR_ENTRY], offset[MAX_GHR_ENTRY];
  int delta[MAX_GHR_ENTRY];

  GLOBAL_REGISTER()
  {
    pf_useful = 0;
    pf_issued = 0;
    global_accuracy = 0;

    for (uint32_t i = 0; i < MAX_GHR_ENTRY; i++) {
      valid[i] = 0;
      sig[i] = 0;
      confidence[i] = 0;
      offset[i] = 0;
      delta[i] = 0;
    }
  }

  void update_entry(uint32_t pf_sig, uint32_t pf_confidence, uint32_t pf_offset, int pf_delta);
  uint32_t check_entry(uint32_t page_offset);
};
} // namespace spp

#endif
