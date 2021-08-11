#include "vmem.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <numeric>

#include "champsim.h"
#include "util.h"

VirtualMemory::VirtualMemory(uint32_t, uint64_t capacity, uint64_t pg_size, uint32_t page_table_levels, uint64_t random_seed)
    : pt_levels(page_table_levels), page_size(pg_size), ppage_free_list((capacity-VMEM_RESERVE_CAPACITY)/PAGE_SIZE, PAGE_SIZE)
{
    assert(capacity % PAGE_SIZE == 0);
    assert(pg_size == (1ul << lg2(pg_size)) && pg_size > 1024);

    // populate the free list
    ppage_free_list.front() = VMEM_RESERVE_CAPACITY;
    std::partial_sum(std::cbegin(ppage_free_list), std::cend(ppage_free_list), std::begin(ppage_free_list));

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

    next_pte_page = ppage_free_list.front();
    ppage_free_list.pop_front();
}

uint64_t VirtualMemory::shamt(uint32_t level) const
{
    return LOG2_PAGE_SIZE + lg2(page_size/PTE_BYTES)*(level);
}

uint64_t VirtualMemory::get_offset(uint64_t vaddr, uint32_t level) const
{
    return (vaddr >> shamt(level)) & bitmask(lg2(page_size/PTE_BYTES));
}

uint64_t VirtualMemory::va_to_pa(uint32_t cpu_num, uint64_t vaddr)
{
    auto [ppage, fault] = vpage_to_ppage_map.insert({{cpu_num, vaddr >> LOG2_PAGE_SIZE}, ppage_free_list.front()});

    // this vpage doesn't yet have a ppage mapping
    if (fault)
        ppage_free_list.pop_front();

    return splice_bits(ppage->second, vaddr, LOG2_PAGE_SIZE);
}

uint64_t VirtualMemory::get_pte_pa(uint32_t cpu_num, uint64_t vaddr, uint32_t level)
{
    std::tuple key{cpu_num, vaddr >> shamt(level+1), level};
    auto [ppage, fault] = page_table.insert({key, next_pte_page});

    // this PTE doesn't yet have a mapping
    if (fault)
    {
        next_pte_page += page_size;
        if (next_pte_page % PAGE_SIZE)
        {
            next_pte_page = ppage_free_list.front();
            ppage_free_list.pop_front();
        }
    }

    return splice_bits(ppage->second, get_offset(vaddr, level) * PTE_BYTES, lg2(page_size));
}

