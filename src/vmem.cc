#include "vmem.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <numeric>

#include "champsim.h"
#include "util.h"

VirtualMemory::VirtualMemory(uint32_t, uint64_t capacity, uint64_t pg_size, uint32_t page_table_levels, uint64_t random_seed)
    : page_size(pg_size), ppage_free_list((capacity-VMEM_RESERVE_CAPACITY)/page_size, page_size), pt_levels(page_table_levels)
{
    assert(capacity % page_size == 0);
    assert(pg_size == (1ul << lg2(pg_size)) && pg_size > 1024);

    std::cout << std::endl;
    std::cout << "VirtualMemory physical capacity: " << capacity << " num_ppages: " << (capacity-VMEM_RESERVE_CAPACITY)/page_size << std::endl;
    std::cout << "VirtualMemory page size: " << page_size << " log2_page_size: " << lg2(page_size) << std::endl;

    std::cout << "VirtualMemory initalizing ppage free list ... " << std::flush;

    // populate the free list
    ppage_free_list.front() = VMEM_RESERVE_CAPACITY;
    std::partial_sum(std::cbegin(ppage_free_list), std::cend(ppage_free_list), std::begin(ppage_free_list));

    std::cout << "done" << std::endl;
    std::cout << "VirtualMemory shuffling ppage free list ... " << std::flush;

    // then shuffle it
    // initialize random number generator
    uint64_t rand_state = random_seed+VMEM_RAND_FACTOR;
    if (rand_state == 0) rand_state = VMEM_RAND_FACTOR<<1;
    for (auto &x : ppage_free_list)
    {
        rand_state += VMEM_RAND_FACTOR;

        rand_state ^= (rand_state<<13);
        rand_state ^= (rand_state>>7);
        rand_state ^= (rand_state<<11);
        rand_state ^= (rand_state>>1);

        std::swap(x, ppage_free_list[rand_state % std::size(ppage_free_list)]);
    }

    std::cout << "done" << std::endl;
    std::cout << std::endl;
}

uint64_t VirtualMemory::va_to_pa(uint32_t cpu_num, uint64_t vaddr)
{
    auto [ppage, fault] = vpage_to_ppage_map.insert({{cpu_num, vaddr >> lg2(page_size)}, ppage_free_list.front()});

    // this vpage doesn't yet have a ppage mapping
    if (fault)
        ppage_free_list.pop_front();

    return splice_bits(ppage->second, vaddr, lg2(page_size));
}

uint64_t VirtualMemory::get_pte_pa(uint32_t cpu_num, uint64_t vaddr, uint32_t level)
{
    std::tuple key{cpu_num, vaddr >> (lg2(page_size) + (9*(pt_levels-level))), level};
    auto [ppage, fault] = page_table.insert({key, ppage_free_list.front()});

    // this PTE doesn't yet have a mapping
    if (fault)
        ppage_free_list.pop_front();

    return splice_bits(ppage->second, (vaddr >> lg2(page_size)) << 3, lg2(page_size));
}

