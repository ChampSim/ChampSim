#include "spp.h"

#include <algorithm>
#include <cassert>

uint64_t spp_hash(uint64_t key)
{
    // Robert Jenkins' 32 bit mix function
    key += (key << 12);
    key ^= (key >> 22);
    key += (key << 4);
    key ^= (key >> 9);
    key += (key << 10);
    key ^= (key >> 2);
    key += (key << 7);
    key ^= (key >> 12);

    // Knuth's multiplicative method
    key = (key >> 3) * 2654435761;

    return key;
}

uint32_t spp_generate_signature(uint32_t last_sig, int32_t delta)
{
    // Sign-magnitude representation
    int32_t sig_delta = (delta < 0) ? (((-1) * delta) + (1 << (SIG_DELTA_BIT - 1))) : delta;
    return ((last_sig << SIG_SHIFT) ^ sig_delta) & bitmask(SIG_BIT);
}

std::tuple<bool, uint32_t, uint32_t, int32_t> SIGNATURE_TABLE::read_and_update_sig(uint64_t addr)
{
    uint64_t page = addr >> LOG2_PAGE_SIZE;
    uint32_t page_offset = (addr & bitmask(LOG2_PAGE_SIZE)) >> LOG2_BLOCK_SIZE;
    uint32_t partial_page = page & bitmask(ST_TAG_BIT);

    auto &set = sigtable[spp_hash(page) % sigtable.size()];

    // Try to find a hit in the set
    auto match_way = std::find_if(std::begin(set), std::end(set), tag_finder<sigtable_entry_t>(partial_page));

    bool hit = (match_way != set.end());
    int32_t delta = 0;

    if (hit)
    {
        delta = page_offset - match_way->last_offset;
    }
    else
    {
        match_way = std::max_element(std::begin(set), std::end(set), lru_comparator<sigtable_entry_t>());

        // Bootstrap the new page
        auto bst_item = std::find_if(std::begin(page_bootstrap_table), std::end(page_bootstrap_table), tag_finder<ghr_entry_t>(page_offset));
        if (bst_item != std::end(page_bootstrap_table))
        {
            match_way->sig = bst_item->sig;
            delta = bst_item->delta;
        }
        else
        {
            match_way->sig = 0;
            delta = 0;
        }
    }

    // Update it with the inbound information
    uint32_t last_sig = match_way->sig;
    match_way->valid        = true;
    match_way->partial_page = partial_page;
    match_way->last_offset  = page_offset;

    if (delta != 0)
        match_way->sig = spp_generate_signature(match_way->sig, delta);

    // Update LRU
    std::for_each(std::begin(set), std::end(set), lru_updater<sigtable_entry_t>(match_way));

    return std::make_tuple(hit, last_sig, match_way->sig, delta);
}

void PATTERN_TABLE::update_pattern(uint32_t last_sig, int curr_delta)
{
    // Update (sig, delta) correlation
    auto &set = pattable[spp_hash(last_sig) % pattable.size()];

    // Check for a hit
    auto match_way = std::find_if(std::begin(set.ways), std::end(set.ways), tag_finder<pattable_entry_t>(curr_delta));

    // Miss
    if (match_way != set.ways.end())
    {
        match_way->c_delta++;
    }
    else
    {
        // Find the smallest delta counter
        match_way = std::min_element(std::begin(set.ways), std::end(set.ways));
        match_way->c_delta = 0;
    }

    match_way->valid = true;
    match_way->delta = curr_delta;

    set.c_sig++;

    // If the signature counter overflows, divide all counters by 2
    if (set.c_sig >= (1 << C_SIG_BIT) || match_way->c_delta >= (1 << C_DELTA_BIT))
    {
        for (auto &it: set.ways)
            it.c_delta >>= 1;
        set.c_sig >>= 1;
    }
}

std::vector<pfqueue_entry_t> PATTERN_TABLE::lookahead(uint32_t curr_sig, int delta)
{
    // Update (sig, delta) correlation
    uint32_t lookahead_conf = GLOBAL_ACCURACY_BIAS;
    uint32_t lookahead_sig = curr_sig;
    int      depth = 0;

    std::vector<pfqueue_entry_t> prefetches;

    while (lookahead_conf > fill_threshold)
    {
        // Index the table for this step
        auto &set = pattable[spp_hash(lookahead_sig) % pattable.size()];

        // Abort if the signature count is zero
        if (set.c_sig == 0)
            break;

        // Get the confidence for all next steps
        for (auto &way : set.ways)
            way.confidence = (depth>0 ? global_accuracy : GLOBAL_ACCURACY_BIAS) * way.c_delta * lookahead_conf / ((set.c_sig + depth) * GLOBAL_ACCURACY_BIAS);

        // Find the maximum confidence
        auto max_conf  = std::max_element(std::begin(set.ways), std::end(set.ways), [](pattable_entry_t x, pattable_entry_t y){ return x.confidence < y.confidence; });
        lookahead_conf = max_conf->confidence;

        assert(lookahead_conf <= GLOBAL_ACCURACY_BIAS);

        // Issue the prefetch to the queue
        if (lookahead_conf > fill_threshold)
        {
#ifndef SPP_NO_SHOTGUN
            for (auto way : set.ways)
                prefetches.emplace_back(lookahead_sig, way.delta, depth, way.confidence);
#else
            prefetches.emplace_back(lookahead_sig, max_conf->delta, depth, lookahead_conf);
#endif
        }

        // Get the next signature
        lookahead_sig = spp_generate_signature(lookahead_sig, max_conf->delta);
        depth++;
    }

    return prefetches;
}

