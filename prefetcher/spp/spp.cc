#include "spp.h"
#include "cache.h"

#include <iostream>
#include <map>

std::map<std::pair<CACHE*, uint8_t>, SIGNATURE_TABLE>     ST;
std::map<std::pair<CACHE*, uint8_t>, PATTERN_TABLE>       PT;
std::map<std::pair<CACHE*, uint8_t>, SPP_PREFETCH_FILTER> FILTER;

void CACHE::l2c_prefetcher_initialize()
{
    std::cout << std::endl;
    std::cout << "CPU " << cpu << " Signature Path Prefetcher" << std::endl;
    std::cout << "Signature table" << " sets: " << ST_SET << " ways: " << ST_WAY << std::endl;
    std::cout << "Pattern table" << " sets: " << PT_SET << " ways: " << PT_WAY << std::endl;
    std::cout << "Prefetch filter" << " sets: " << FILTER_SET << " ways: " << FILTER_WAY << std::endl;
    std::cout << "Fill threshold: " << PT[std::make_pair(this, cpu)].fill_threshold << std::endl;
    std::cout << "High-confidence threshold: " << FILTER[std::make_pair(this, cpu)].highconf_threshold << std::endl;
    std::cout << std::endl;
}

uint32_t CACHE::l2c_prefetcher_operate(uint64_t base_addr, uint64_t ip, uint8_t cache_hit, uint8_t type, uint32_t metadata_in)
{
    auto pf_issued = FILTER[std::make_pair(this, cpu)].pf_useful + FILTER[std::make_pair(this, cpu)].pf_useless;
    PT[std::make_pair(this, cpu)].global_accuracy = pf_issued ? ((GLOBAL_ACCURACY_BIAS * FILTER[std::make_pair(this, cpu)].pf_useful) / pf_issued) : 0;

    FILTER[std::make_pair(this, cpu)].update_demand(base_addr);

    // Stage 1: Read and update a sig stored in ST
    // last_sig and delta are used to update (sig, delta) correlation in PT
    // curr_sig is used to read prefetch candidates in PT
    bool     st_hit;
    uint32_t last_sig, curr_sig;
    int32_t  delta;
    std::tie(st_hit, last_sig, curr_sig, delta) = ST[std::make_pair(this, cpu)].read_and_update_sig(base_addr);

    // Stage 2: Update delta patterns stored in PT
    // If we miss the ST, we skip this update, because the pattern won't be present
    if (st_hit) PT[std::make_pair(this, cpu)].update_pattern(last_sig, delta);

    // Stage 3: Start prefetching
    uint64_t pf_addr = base_addr & ~bitmask(LOG2_BLOCK_SIZE);

    // Read the PT. This iterates down the path until the confidence sinks below FILL_THRESHOLD
    std::vector<pfqueue_entry_t> path = PT[std::make_pair(this, cpu)].lookahead(curr_sig, delta);

    //At this point, the path is determined, but the particular addresses are not resolved.
    // Iterate through the prefetch path
    for (const auto &pf : path)
    {
        // Resolve the address of the prefetched block
        pf_addr += (pf.delta << LOG2_BLOCK_SIZE);

        // Prefetch request is crossing the physical page boundary
        if ((base_addr>>LOG2_PAGE_SIZE) == (pf_addr>>LOG2_PAGE_SIZE))
        {
            // Check the filter
            confidence_t fill_level = FILTER[std::make_pair(this, cpu)].check(pf_addr, pf.confidence);
            if (fill_level != REJECT)
            {
                // Issue the prefetch. If this fails, the queue was full.
                bool prefetched = prefetch_line(ip, base_addr, pf_addr, ((fill_level == STRONGLY_ACCEPT) ? FILL_L2 : FILL_LLC),0);
                if (prefetched)
                    FILTER[std::make_pair(this, cpu)].update_issue(pf_addr);
            }
        }
        else
        {
            // Store this prefetch request in SGHR to bootstrap SPP learning when we see a ST miss accessing a new page
            // NOTE: SGHR implementation is slightly different from the original paper
            // Instead of matching (last_offset + delta), SGHR simply stores and matches the pf_offset

            // Find the item in the table
            auto pf_offset = (pf_addr & bitmask(LOG2_PAGE_SIZE)) >> LOG2_BLOCK_SIZE;
            auto bst_item = std::find_if(std::begin(ST[std::make_pair(this, cpu)].page_bootstrap_table), std::end(ST[std::make_pair(this, cpu)].page_bootstrap_table), tag_finder<SIGNATURE_TABLE::ghr_entry_t>(pf_offset));

            // If not found, find an invalid or lowest-confidence way to replace
            if (bst_item == std::end(ST[std::make_pair(this, cpu)].page_bootstrap_table))
                bst_item = std::min_element(std::begin(ST[std::make_pair(this, cpu)].page_bootstrap_table), std::end(ST[std::make_pair(this, cpu)].page_bootstrap_table));

            // Place the information in the table
            bst_item->valid = true;
            bst_item->sig = pf.sig;
            bst_item->confidence = pf.confidence;
            bst_item->offset = pf_offset;
            bst_item->delta = pf.delta;
            break;
        }
    }

    return metadata_in;
}

uint32_t CACHE::l2c_prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
    FILTER[std::make_pair(this, cpu)].update_evict(evicted_addr);
    return metadata_in;
}

void CACHE::l2c_prefetcher_final_stats()
{
}

