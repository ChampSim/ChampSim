#include "bytecode_buffer.h"
#include <algorithm>
#include <random>
void BYTECODE_BUFFER::initialize() {
    for (uint8_t i = 0; i < static_cast<uint8_t>(BYTECODE_BUFFER_NUM); i++) {
        buffers.push_back(BB_ENTRY{i});
    }
}

bool BYTECODE_BUFFER::hitInBB(uint64_t sourceMemoryAddr) {
    BB_ENTRY* entry = hit(sourceMemoryAddr);
    if (entry == nullptr) {
        stats.miss++;
        if constexpr (BB_DEBUG_LEVEL > 1) fmt::print("[BYTECODE BUFFER] Missed on address {} - Total misses in BB: {} -> {}%\n", sourceMemoryAddr, stats.miss, (stats.miss * 100)/(stats.miss + stats.hits));
        return false;
    }
    stats.hits++;
    if constexpr (BB_DEBUG_LEVEL > 1) fmt::print("[BYTECODE BUFFER] Hit on address {} - Total hits in BB: {} -> {}%\n", sourceMemoryAddr, stats.hits, (stats.hits * 100)/(stats.miss + stats.hits));
    decrementLRUs();
    entry->lru++;
    return true;
}

bool BYTECODE_BUFFER::shouldFetch(uint64_t baseAddr) {
    for (BB_ENTRY& entry : buffers) {
        if constexpr (BB_DEBUG_LEVEL > 2) 
            fmt::print("[BYTECODE BUFFER] Checking fetching on entry, fetching {}, lru {}, valid {}, baseaddr {}, maxaddr {} \n", entry.fetching, entry.lru, entry.valid, entry.baseAddr, entry.maxAddr);
        if (entry.hit(baseAddr) && !entry.fetching) {
            return false;
        }
        if (entry.currentlyFetching(baseAddr)) {
            return false;
        }
    }
    return true;
}

void BYTECODE_BUFFER::fetching(uint64_t baseAddr, uint64_t currentCycle) {
    auto victim = find_victim();
    if (victim != nullptr) {
        victim->prefetch(baseAddr, currentCycle);
        if constexpr (BB_DEBUG_LEVEL > 2) fmt::print("[BYTECODE BUFFER] Starting fetching in BB: {}, victim: {},  \n", baseAddr, victim->index);
        stats.prefetches++;
        return;
    }
    
    fmt::print(stderr, "[BYTECODE BUFFER] Found no vitim {} \n", baseAddr);
}

void BYTECODE_BUFFER::updateBufferEntry(uint64_t baseAddr, uint64_t currentCycle) {
        for (BB_ENTRY& entry : buffers) {
        if constexpr (BB_DEBUG_LEVEL > 2) fmt::print("[BYTECODE BUFFER] Checking updating on entry, fetching {}, lru {}, valid {}, baseaddr {}, maxaddr {} \n", entry.fetching, entry.lru, entry.valid, entry.baseAddr, entry.maxAddr);
        if (entry.fetching && (baseAddr == entry.fetching_base_addr + (FETCH_OFFSET * BYTECODE_SIZE) || baseAddr + 1 == entry.fetching_base_addr + (FETCH_OFFSET * BYTECODE_SIZE))){
            if (!entry.fetching) return; 

            if constexpr (BB_DEBUG_LEVEL > 2) fmt::print("[BYTECODE BUFFER] Correctly updating in BB: {} \n", baseAddr);
            stats.totalMissWait += currentCycle - entry.fetchingEventCycle;
            entry.valid = true;
            entry.fetching = false;
            entry.baseAddr = entry.fetching_base_addr;
            entry.maxAddr = entry.fetching_max_addr;
            entry.lru = STARTING_LRU_VAL;
            decrementLRUs();
            entry.lru++;
            return;
        } 
    }
    
    

    if (hit(baseAddr)) return;
    fmt::print(stderr, "[BYTECODE BUFFER] Uncorrectly updating in BB: {}, cycle: {} \n", baseAddr, currentCycle);
    printInterestingThings();
}


void BYTECODE_BUFFER::decrementLRUs() {
    for (BB_ENTRY& entry : buffers) { if (entry.valid && entry.lru > 0) entry.lru--; }
}

BB_ENTRY* BYTECODE_BUFFER::hit(uint64_t sourceMemoryAddr) {
    for (BB_ENTRY& entry : buffers) {
        if (entry.hit(sourceMemoryAddr)) {
            return const_cast<BB_ENTRY*>(&entry);
        } 
    }
    return nullptr;
}

BB_ENTRY* BYTECODE_BUFFER::find_victim() {
    auto initial_entry = std::find_if(buffers.begin(), buffers.end(), [] (BB_ENTRY entry) { return !entry.fetching; }); 
    if (initial_entry == buffers.end()) {
        fmt::print(stderr, "[BYTECODE BUFFER] not enough lines for all prefetches \n");
        return nullptr;
    } else {
        auto victim = &(*initial_entry);
        for (auto &entry : buffers) {
            if (!entry.fetching && (entry.lru < victim->lru)) {
                victim = &entry;
            }
        }
        return victim;
    }

}

void BYTECODE_BUFFER::printInterestingThings() {
    for (BB_ENTRY const &entry : buffers) {
        fmt::print(stderr, "[{}] Times changed out {}, times reset: {}, lru: {}, fetching: {}, valid: {}, currentAddr: {}, currentMaxAddr: {}, fetching_base: {}, fetching_max: {}, fetching_cycle: {} \n", entry.index, entry.timesSwitchedOut, entry.timesReset, entry.lru, entry.fetching, entry.valid, entry.baseAddr, entry.maxAddr, entry.fetching_base_addr, entry.fetching_max_addr, entry.fetchingEventCycle);
    }
}