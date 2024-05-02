#ifdef CHAMPSIM_MODULE
#define SET_ASIDE_CHAMPSIM_MODULE
#undef CHAMPSIM_MODULE
#endif

#ifndef BYTECODE_BUFFER_H
#define BYTECODE_BUFFER_H

#include <array>
#include <bitset>
#include <deque>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <map>
#include <fmt/core.h>
#include <fmt/ranges.h>

#include "champsim.h"
#include "champsim_constants.h"
#include "channel.h"
#include "cache.h"
#include "operable.h"
#include <type_traits>

constexpr std::size_t BYTECODE_SIZE = 2;
constexpr std::size_t BYTECODE_BUFFER_SIZE = 64;
constexpr std::size_t BYTECODE_BUFFER_NUM = 3;
constexpr uint64_t BYTECODE_FETCH_TIME = 4;
constexpr uint64_t BYTECODE_BRANCH_MISPREDICT_PENALTY = 4;
constexpr uint64_t BB_DEBUG_LEVEL = 0; // 0 = NONE, 1 = WARNINGS, 2 = HITS AND MISSES, 3 = INFO 
constexpr int STARTING_LRU_VAL = std::numeric_limits<int>::max();

struct BB_STATS {
    uint64_t hits;
    uint64_t miss;
    uint64_t totalMissWait;
    double averageWaitTime() const { return (double) totalMissWait/ (double) miss; }
};

struct BB_ENTRY {
    uint64_t baseAddr;
    uint64_t maxAddr; 
    uint64_t fetchingEventCycle = 0;
    bool valid = false;
    bool fetching = false;
    int lru = 0; 

    bool hit(uint64_t sourceAddr) const { return ((sourceAddr >= baseAddr) && (sourceAddr <= maxAddr) && valid); }
    bool currentlyFetching(uint64_t sourceAddr) const { return ((sourceAddr >= baseAddr) && (sourceAddr <= maxAddr) && fetching); }
    void prefetch(uint64_t sourceAddr, uint64_t currentCycle) {
        fetching = true;
        valid = false;
        lru = STARTING_LRU_VAL;
        baseAddr = sourceAddr;
        maxAddr = sourceAddr + (BYTECODE_BUFFER_SIZE * BYTECODE_SIZE);
        fetchingEventCycle = currentCycle;
    }
    void reset() {
        valid = false;
        fetching = false;
        lru = 0;
    }
};

class BYTECODE_BUFFER {
    std::vector<BB_ENTRY> buffers; 

    void decrementLRUs();
    BB_ENTRY* hit(uint64_t sourceMemoryAddr);
    BB_ENTRY* find_victim();

 public:
    BB_STATS stats;

    void initialize();
    bool hitInBB(uint64_t sourceMemoryAddr);
    bool shouldFetch(uint64_t sourceMemoryAddr, uint64_t currentCycle);
    void updateBufferEntry(uint64_t baseAddr, uint64_t currentCycle);
};


#endif
