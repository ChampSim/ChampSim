#include <algorithm>
#include <array>
#include <map>

#include "cache.h"

constexpr int PREFETCH_DEGREE = 3;

struct tracker_entry
{
    uint64_t ip           = 0; // the IP we're tracking
    uint64_t last_cl_addr = 0; // the last address accessed by this IP
    int64_t last_stride   = 0; // the stride between the last two addresses accessed by this IP
    uint64_t lru          = 0; // use LRU to evict old IP trackers
};

constexpr std::size_t TRACKER_SETS = 256;
constexpr std::size_t TRACKER_WAYS = 4;
std::map<CACHE*, std::tuple<uint64_t, int64_t, int>> lookahead; // pf_address, stride, degree
std::map<CACHE*, std::array<tracker_entry, TRACKER_SETS*TRACKER_WAYS>> trackers;

void CACHE::l2c_prefetcher_initialize()
{
    std::cout << this->NAME << " IP-based stride prefetcher" << std::endl;
}

void CACHE::prefetcher_cycle_operate()
{
    // If a lookahead is active
    if (auto [pf_address, stride, degree] = lookahead[this]; pf_address != 0)
    {
        // If the next step would exceed the degree or run off the page, stop
        if (degree < PREFETCH_DEGREE && (virtual_prefetch || ((pf_address + (stride << LOG2_BLOCK_SIZE)) >> LOG2_PAGE_SIZE) == (pf_address >> LOG2_PAGE_SIZE)))
        {
            // calculate prefetch address
            pf_address += stride << LOG2_BLOCK_SIZE;

            // check the MSHR occupancy to decide if we're going to prefetch to the L2 or LLC
            prefetch_line(0, 0, pf_address, (get_occupancy(0,pf_address) < get_size(0,pf_address)/2) ? FILL_L2 : FILL_LLC, 0);
        }
        else
        {
            pf_address = 0;
        }

        lookahead[this] = {pf_address, stride, degree+1};
    }
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type, uint32_t metadata_in)
{
    uint64_t cl_addr = addr >> LOG2_BLOCK_SIZE;
    int64_t stride = 0;

    // get boundaries of tracking set
    auto set_begin = std::next(std::begin(trackers[this]), ip % TRACKER_SETS);
    auto set_end   = std::next(set_begin, TRACKER_WAYS);

    // find the current ip within the set
    auto found = std::find_if(set_begin, set_end, [ip](tracker_entry x){ return x.ip == ip; });

    // if we found a matching entry
    if (found != set_end)
    {
        // calculate the stride between the current address and the last address
        // no need to check for overflow since these values are downshifted
        stride = (int64_t)cl_addr - (int64_t)found->last_cl_addr;

        // Initialize prefetch state unless we somehow saw the same address twice in a row
        // or if this is the first time we've seen this stride
        if (stride != 0 && stride == found->last_stride)
            lookahead[this] = {cl_addr << LOG2_BLOCK_SIZE, stride, 0};
    }
    else
    {
        // replace by LRU
        found = std::max_element(set_begin, set_end, [](tracker_entry x, tracker_entry y){ return x.lru < y.lru; });
    }

    // update tracking set
    std::for_each(set_begin, set_end, [](tracker_entry &x){ x.lru++; });
    *found = {ip, cl_addr, stride, 0};

    return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
    return metadata_in;
}

void CACHE::prefetcher_final_stats()
{
}

