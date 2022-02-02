#ifndef DEFAULTS_HPP
#define DEFAULTS_HPP

#include "cache.h"
#include "champsim_constants.h"
#include "ooo_cpu.h"

namespace champsim
{
namespace defaults
{
    const auto default_core = O3_CPU::Builder{}
        .ifetch_buffer_size(64)
        .decode_buffer_size(32)
        .dispatch_buffer_size(32)
        .rob_size(352)
        .lq_size(128)
        .sq_size(72)
        .fetch_width(6)
        .decode_width(6)
        .dispatch_width(6)
        .execute_width(4)
        .lq_width(2)
        .sq_width(2)
        .retire_width(5)
        .mispredict_penalty(1)
        .schedule_width(128)
        .decode_latency(1)
        .dispatch_latency(1)
        .schedule_latency(0)
        .execute_latency(0);

    const auto default_l1i = CACHE::Builder{}
        .sets(64)
        .ways(8)
        .pq_size(32)
        .mshr_size(8)
        .hit_latency(3)
        .fill_latency(1)
        .max_read(2)
        .max_write(2)
        .set_prefetch_as_load(false)
        .set_virtual_prefetch(true)
        .set_wq_checks_full_addr(true)
        .prefetch_activate((1 << LOAD) | (1 << PREFETCH))
        // Specifying default prefetchers and replacement policies like this is probably dangerous
        // since the names could change.
        // We're doing it anyway, for now.
        .prefetcher(CACHE::pprefetcherDno_instr)
        .replacement(CACHE::rreplacementDlru);

    const auto default_l1d = CACHE::Builder{}
        .sets(64)
        .ways(12)
        .pq_size(8)
        .mshr_size(16)
        .hit_latency(4)
        .fill_latency(1)
        .max_read(2)
        .max_write(2)
        .set_prefetch_as_load(false)
        .set_virtual_prefetch(false)
        .set_wq_checks_full_addr(true)
        .prefetch_activate((1 << LOAD) | (1 << PREFETCH))
        .prefetcher(CACHE::pprefetcherDno)
        .replacement(CACHE::rreplacementDlru);

    const auto default_l2c = CACHE::Builder{}
        .sets(1024)
        .ways(8)
        .pq_size(16)
        .mshr_size(32)
        .hit_latency(9)
        .fill_latency(1)
        .max_read(1)
        .max_write(1)
        .set_prefetch_as_load(false)
        .set_virtual_prefetch(false)
        .set_wq_checks_full_addr(false)
        .prefetch_activate((1 << LOAD) | (1 << PREFETCH))
        .prefetcher(CACHE::pprefetcherDno)
        .replacement(CACHE::rreplacementDlru);

    const auto default_itlb = CACHE::Builder{}
        .sets(16)
        .ways(4)
        .pq_size(0)
        .mshr_size(8)
        .hit_latency(1)
        .fill_latency(1)
        .max_read(2)
        .max_write(2)
        .set_prefetch_as_load(false)
        .set_virtual_prefetch(true)
        .set_wq_checks_full_addr(true)
        .prefetch_activate((1 << LOAD) | (1 << PREFETCH))
        .prefetcher(CACHE::pprefetcherDno)
        .replacement(CACHE::rreplacementDlru);

    const auto default_dtlb = CACHE::Builder{}
        .sets(16)
        .ways(4)
        .pq_size(0)
        .mshr_size(8)
        .hit_latency(1)
        .fill_latency(1)
        .max_read(2)
        .max_write(2)
        .set_prefetch_as_load(false)
        .set_virtual_prefetch(false)
        .set_wq_checks_full_addr(true)
        .prefetch_activate((1 << LOAD) | (1 << PREFETCH))
        .prefetcher(CACHE::pprefetcherDno)
        .replacement(CACHE::rreplacementDlru);

    const auto default_stlb = CACHE::Builder{}
        .sets(128)
        .ways(12)
        .pq_size(0)
        .mshr_size(16)
        .hit_latency(7)
        .fill_latency(1)
        .max_read(1)
        .max_write(1)
        .set_prefetch_as_load(false)
        .set_virtual_prefetch(false)
        .set_wq_checks_full_addr(false)
        .prefetch_activate((1 << LOAD) | (1 << PREFETCH))
        .prefetcher(CACHE::pprefetcherDno)
        .replacement(CACHE::rreplacementDlru);

    const auto default_llc = CACHE::Builder{}
        .name("LLC")
        .sets(2048*NUM_CPUS)
        .ways(16)
        .pq_size(32*NUM_CPUS)
        .mshr_size(64*NUM_CPUS)
        .hit_latency(19)
        .fill_latency(1)
        .max_read(NUM_CPUS)
        .max_write(NUM_CPUS)
        .set_prefetch_as_load(false)
        .set_virtual_prefetch(false)
        .set_wq_checks_full_addr(false)
        .prefetch_activate((1 << LOAD) | (1 << PREFETCH))
        .prefetcher(CACHE::pprefetcherDno)
        .replacement(CACHE::rreplacementDlru);
}
}

#endif

