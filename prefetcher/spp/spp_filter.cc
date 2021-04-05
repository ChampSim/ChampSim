#include "spp.h"

#include <algorithm>
#include <cassert>

confidence_t SPP_PREFETCH_FILTER::check(uint64_t check_addr, unsigned int confidence)
{
    uint64_t page_no = check_addr >> LOG2_PAGE_SIZE;
    uint64_t line_no = (check_addr & bitmask(LOG2_PAGE_SIZE)) >> LOG2_BLOCK_SIZE;

    auto &set = prefetch_table[spp_hash(page_no) % prefetch_table.size()];
    auto match_way = std::find_if(std::begin(set), std::end(set), tag_finder<filter_entry_t>(page_no));

    if (match_way != std::end(set) && match_way->prefetched.test(line_no))
        return REJECT;

    return confidence >= highconf_threshold ? STRONGLY_ACCEPT : WEAKLY_ACCEPT;
}

void SPP_PREFETCH_FILTER::update_demand(uint64_t check_addr)
{
    uint64_t page_no = check_addr >> LOG2_PAGE_SIZE;
    uint64_t line_no = (check_addr & bitmask(LOG2_PAGE_SIZE)) >> LOG2_BLOCK_SIZE;

    auto set = prefetch_table[spp_hash(page_no) % prefetch_table.size()];
    auto match_way = std::find_if(std::begin(set), std::end(set), tag_finder<filter_entry_t>(page_no));

    if (match_way != std::end(set))
    {
        if (match_way->prefetched.test(line_no) && !match_way->used[line_no])
            pf_useful++;

        // Mark line as used
        match_way->used.set(line_no);

        // Update LRU
        std::for_each(std::begin(set), std::end(set), lru_updater<filter_entry_t>(match_way));
    }

    // Handle counter overflow
    if (pf_useful >= (1 << GLOBAL_COUNTER_BIT))
    {
        pf_useful >>= 1;
        pf_useless >>= 1;
    }
}

void SPP_PREFETCH_FILTER::update_evict(uint64_t check_addr)
{
    uint64_t page_no = check_addr >> LOG2_PAGE_SIZE;
    uint64_t line_no = (check_addr & bitmask(LOG2_PAGE_SIZE)) >> LOG2_BLOCK_SIZE;

    auto &set = prefetch_table[spp_hash(page_no) % prefetch_table.size()];
    auto match_way = std::find_if(std::begin(set), std::end(set), tag_finder<filter_entry_t>(page_no));

    if (match_way != std::end(set))
    {
        if (match_way->prefetched.test(line_no) && !match_way->used[line_no])
            pf_useless++;

        // Reset filter entry
        match_way->prefetched.reset(line_no);
        match_way->used.reset(line_no);

        // Update LRU
        std::for_each(std::begin(set), std::end(set), lru_updater<filter_entry_t>(match_way));
    }

    // Handle counter overflow
    if (pf_useless >= (1 << GLOBAL_COUNTER_BIT))
    {
        pf_useful >>= 1;
        pf_useless >>= 1;
    }
}

void SPP_PREFETCH_FILTER::update_issue(uint64_t check_addr)
{
    uint64_t page_no = check_addr >> LOG2_PAGE_SIZE;
    uint64_t line_no = (check_addr & bitmask(LOG2_PAGE_SIZE)) >> LOG2_BLOCK_SIZE;

    auto &set = prefetch_table[spp_hash(page_no) % prefetch_table.size()];
    auto match_way = std::find_if(std::begin(set), std::end(set), tag_finder<filter_entry_t>(page_no));

    if (match_way == std::end(set))
    {
        match_way = std::max_element(std::begin(set), std::end(set), lru_comparator<filter_entry_t>());
        match_way->prefetched.reset();
        match_way->used.reset();
    }

    assert(line_no < match_way->prefetched.size());
    match_way->page_no = page_no;
    match_way->prefetched.set(line_no);
    match_way->used.reset(line_no);

    // Update LRU
    std::for_each(std::begin(set), std::end(set), lru_updater<filter_entry_t>(match_way));
}

