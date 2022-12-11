#include <algorithm>
#include <array>
#include <iostream>
#include <map>
#include <optional>

#include "cache.h"
#include "msl/lru_table.h"

namespace
{
struct tracker {
  struct tracker_entry {
    champsim::address ip{};           // the IP we're tracking
    champsim::block_number last_cl_addr{}; // the last address accessed by this IP
    champsim::block_number::difference_type last_stride{};   // the stride between the last two addresses accessed by this IP

    auto index() const { return ip.slice_upper<2>(); }
    auto tag() const { return ip.slice_upper<2>(); }
  };

  struct lookahead_entry {
    champsim::address address{};
    champsim::address::difference_type stride{};
    int degree = 0; // degree remaining
  };

  constexpr static std::size_t TRACKER_SETS = 256;
  constexpr static std::size_t TRACKER_WAYS = 4;
  constexpr static int PREFETCH_DEGREE = 3;

  std::optional<lookahead_entry> active_lookahead;

  champsim::msl::lru_table<tracker_entry> table{TRACKER_SETS, TRACKER_WAYS};

public:
  void initiate_lookahead(champsim::address ip, champsim::block_number cl_addr)
  {
    champsim::block_number::difference_type stride = 0;

    auto found = table.check_hit({ip, cl_addr, stride});

    // if we found a matching entry
    if (found.has_value()) {
      stride = champsim::offset(found->last_cl_addr, cl_addr);

      // Initialize prefetch state unless we somehow saw the same address twice in
      // a row or if this is the first time we've seen this stride
      if (stride != 0 && stride == found->last_stride)
        active_lookahead = {champsim::address{cl_addr}, stride, PREFETCH_DEGREE};
    }

    // update tracking set
    table.fill({ip, cl_addr, stride});
  }

  void advance_lookahead(CACHE* cache)
  {
    // If a lookahead is active
    if (active_lookahead.has_value()) {
      auto [old_pf_address, stride, degree] = active_lookahead.value();
      assert(degree > 0);

      auto pf_address = old_pf_address + stride*BLOCK_SIZE;

      // If the next step would exceed the degree or run off the page, stop
      if (cache->virtual_prefetch || champsim::page_number{pf_address} == champsim::page_number{old_pf_address}) {
        // check the MSHR occupancy to decide if we're going to prefetch to this level or not
        const bool mshr_under_light_load = cache->get_occupancy(0, pf_address) < (cache->get_size(0, pf_address) / 2);
        const bool success = cache->prefetch_line(pf_address, mshr_under_light_load, 0);
        if (success)
          active_lookahead = {pf_address, stride, degree - 1};
        // If we fail, try again next cycle

        if (active_lookahead->degree == 0)
          active_lookahead.reset();
      } else {
        active_lookahead.reset();
      }
    }
  }
};

std::map<CACHE*, tracker> trackers;
} // namespace

void CACHE::prefetcher_initialize() { std::cout << NAME << " IP-based stride prefetcher" << std::endl; }

void CACHE::prefetcher_cycle_operate() { ::trackers[this].advance_lookahead(this); }

uint32_t CACHE::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, uint8_t type, uint32_t metadata_in)
{
  ::trackers[this].initiate_lookahead(ip, champsim::block_number{addr});
  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(champsim::address addr, uint32_t set, uint32_t way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void CACHE::prefetcher_final_stats() {}
