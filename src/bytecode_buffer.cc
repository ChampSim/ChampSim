#include "bytecode_buffer.h"
#include <algorithm>
#include <random>
void BYTECODE_BUFFER::initialize() {
    for (std::size_t i = 0; i < BYTECODE_BUFFER_NUM; i++) {
        buffers.push_back(BB_entry{});
    }
}

bool BYTECODE_BUFFER::hitInBB(uint64_t sourceMemoryAddr) {
    BB_entry* entry = hit(sourceMemoryAddr);
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

bool BYTECODE_BUFFER::shouldFetch(uint64_t baseAddr, uint64_t currentCycle) {
    for (BB_entry& entry : buffers) {
        if constexpr (BB_DEBUG_LEVEL > 2) 
            fmt::print("[BYTECODE BUFFER] Checking fetching on entry, fetching {}, lru {}, valid {}, baseaddr {}, maxaddr {} \n", entry.fetching, entry.lru, entry.valid, entry.baseAddr, entry.maxAddr);
        if (entry.hit(baseAddr) || entry.currentlyFetching(baseAddr)) {
            return false;
        }
    }

    auto victim = find_victim();
    if (victim != nullptr) {
        victim->prefetch(baseAddr, currentCycle);
        if constexpr (BB_DEBUG_LEVEL > 2) fmt::print("[BYTECODE BUFFER] Starting fetching in BB: {} \n", baseAddr);
        return true;
    }
    
    fmt::print("[BYTECODE BUFFER] This is very very wrong {} \n", baseAddr);

    return true;
}

void BYTECODE_BUFFER::updateBufferEntry(uint64_t baseAddr, uint64_t currentCycle) {
    if (hit(baseAddr) != nullptr) {
        for (BB_entry& entry : buffers) {
            if (entry.currentlyFetching(baseAddr)) entry.reset();
        }
        return;
    }
    bool foundDuplicate = false;
    for (BB_entry& entry : buffers) {
        if constexpr (BB_DEBUG_LEVEL > 2) fmt::print("[BYTECODE BUFFER] Checking updating on entry, fetching {}, lru {}, valid {}, baseaddr {}, maxaddr {} \n", entry.fetching, entry.lru, entry.valid, entry.baseAddr, entry.maxAddr);
        if (entry.currentlyFetching(baseAddr) && !foundDuplicate) {
            if constexpr (BB_DEBUG_LEVEL > 2) fmt::print("[BYTECODE BUFFER] Correctly updating in BB: {} \n", baseAddr);
            stats.totalMissWait += currentCycle - entry.fetchingEventCycle;
            entry.valid = true;
            entry.fetching = false;
            decrementLRUs();
            entry.lru++;
            foundDuplicate = true;
        } else if (entry.currentlyFetching(baseAddr) && foundDuplicate) {
            entry.reset();
        } 
    }
    if constexpr (BB_DEBUG_LEVEL) fmt::print("[BYTECODE BUFFER] Uncorrectly updating in BB: {} \n", baseAddr);
}


void BYTECODE_BUFFER::decrementLRUs() {
    for (BB_entry& entry : buffers) { if (entry.valid && entry.lru > 0) entry.lru--; }
}

BB_entry* BYTECODE_BUFFER::hit(uint64_t sourceMemoryAddr) {
    for (BB_entry& entry : buffers) {
        if (entry.hit(sourceMemoryAddr)) {
            return const_cast<BB_entry*>(&entry);
        } 
    }
    return nullptr;
}

BB_entry* BYTECODE_BUFFER::find_victim() {
    if (buffers.empty()) {
        return nullptr;  // Handle empty buffers case
    }

    // Start by assuming the first non-excluded entry as the minimum
    auto minLRU = std::find_if(buffers.begin(), buffers.end(), 
                               [](const BB_entry& entry) {
                                   return !entry.fetching;
                               });

    // If no non-excluded element is found, return nullptr
    if (minLRU == buffers.end()) {
        return nullptr;
    }

    // Continue the search from the next element after the initial found one
    for (auto it = std::next(minLRU); it != buffers.end(); ++it) {
        if (!it->fetching && it->lru < minLRU->lru) {
            minLRU = it;
        }
    }

    return (minLRU != buffers.end()) ? &(*minLRU) : nullptr;
}
