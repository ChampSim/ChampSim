/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef DEFAULTS_HPP
#define DEFAULTS_HPP

#include "cache.h"
#include "champsim_constants.h"
#include "ooo_cpu.h"
#include "ptw.h"

namespace champsim::defaults
{
const auto default_core = O3_CPU::Builder{}
                              .dib_set(32)
                              .dib_way(8)
                              .dib_window(16)
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
                              .execute_latency(0)
                              .l1i_bandwidth(1)
                              .l1d_bandwidth(1)
                              // Specifying default branch predictors and BTBs like this is probably dangerous
                              // since the names could change.
                              // We're doing it anyway, for now.
                              .branch_predictor<O3_CPU::bbranchDbimodal>()
                              .btb<O3_CPU::tbtbDbasic_btb>();

const auto default_l1i = CACHE::Builder{}
                             .sets(64)
                             .ways(8)
                             .pq_size(32)
                             .mshr_size(8)
                             .hit_latency(3)
                             .fill_latency(1)
                             .tag_bandwidth(2)
                             .fill_bandwidth(2)
                             .offset_bits(LOG2_BLOCK_SIZE)
                             .reset_prefetch_as_load()
                             .set_virtual_prefetch()
                             .set_wq_checks_full_addr()
                             .prefetch_activate(access_type::LOAD, access_type::PREFETCH)
                             // Specifying default prefetchers and replacement policies like this is probably dangerous
                             // since the names could change.
                             // We're doing it anyway, for now.
                             .prefetcher<CACHE::pprefetcherDno_instr>()
                             .replacement<CACHE::rreplacementDlru>();

const auto default_l1d = CACHE::Builder{}
                             .sets(64)
                             .ways(12)
                             .pq_size(8)
                             .mshr_size(16)
                             .hit_latency(4)
                             .fill_latency(1)
                             .tag_bandwidth(2)
                             .fill_bandwidth(2)
                             .offset_bits(LOG2_BLOCK_SIZE)
                             .reset_prefetch_as_load()
                             .reset_virtual_prefetch()
                             .set_wq_checks_full_addr()
                             .prefetch_activate(access_type::LOAD, access_type::PREFETCH)
                             .prefetcher<CACHE::pprefetcherDno>()
                             .replacement<CACHE::rreplacementDlru>();

const auto default_l2c = CACHE::Builder{}
                             .sets(1024)
                             .ways(8)
                             .pq_size(16)
                             .mshr_size(32)
                             .hit_latency(9)
                             .fill_latency(1)
                             .tag_bandwidth(1)
                             .fill_bandwidth(1)
                             .offset_bits(LOG2_BLOCK_SIZE)
                             .reset_prefetch_as_load()
                             .reset_virtual_prefetch()
                             .reset_wq_checks_full_addr()
                             .prefetch_activate(access_type::LOAD, access_type::PREFETCH)
                             .prefetcher<CACHE::pprefetcherDno>()
                             .replacement<CACHE::rreplacementDlru>();

const auto default_itlb = CACHE::Builder{}
                              .sets(16)
                              .ways(4)
                              .pq_size(0)
                              .mshr_size(8)
                              .hit_latency(1)
                              .fill_latency(1)
                              .tag_bandwidth(2)
                              .fill_bandwidth(2)
                              .offset_bits(LOG2_PAGE_SIZE)
                              .reset_prefetch_as_load()
                              .set_virtual_prefetch()
                              .set_wq_checks_full_addr()
                              .prefetch_activate(access_type::LOAD, access_type::PREFETCH)
                              .prefetcher<CACHE::pprefetcherDno>()
                              .replacement<CACHE::rreplacementDlru>();

const auto default_dtlb = CACHE::Builder{}
                              .sets(16)
                              .ways(4)
                              .pq_size(0)
                              .mshr_size(8)
                              .hit_latency(1)
                              .fill_latency(1)
                              .tag_bandwidth(2)
                              .fill_bandwidth(2)
                              .offset_bits(LOG2_PAGE_SIZE)
                              .reset_prefetch_as_load()
                              .reset_virtual_prefetch()
                              .set_wq_checks_full_addr()
                              .prefetch_activate(access_type::LOAD, access_type::PREFETCH)
                              .prefetcher<CACHE::pprefetcherDno>()
                              .replacement<CACHE::rreplacementDlru>();

const auto default_stlb = CACHE::Builder{}
                              .sets(128)
                              .ways(12)
                              .pq_size(0)
                              .mshr_size(16)
                              .hit_latency(7)
                              .fill_latency(1)
                              .tag_bandwidth(1)
                              .fill_bandwidth(1)
                              .offset_bits(LOG2_PAGE_SIZE)
                              .reset_prefetch_as_load()
                              .reset_virtual_prefetch()
                              .reset_wq_checks_full_addr()
                              .prefetch_activate(access_type::LOAD, access_type::PREFETCH)
                              .prefetcher<CACHE::pprefetcherDno>()
                              .replacement<CACHE::rreplacementDlru>();

const auto default_llc = CACHE::Builder{}
                             .name("LLC")
                             .sets(2048 * NUM_CPUS)
                             .ways(16)
                             .pq_size(32 * NUM_CPUS)
                             .mshr_size(64 * NUM_CPUS)
                             .hit_latency(19)
                             .fill_latency(1)
                             .tag_bandwidth(NUM_CPUS)
                             .fill_bandwidth(NUM_CPUS)
                             .offset_bits(LOG2_BLOCK_SIZE)
                             .reset_prefetch_as_load()
                             .reset_virtual_prefetch()
                             .reset_wq_checks_full_addr()
                             .prefetch_activate(access_type::LOAD, access_type::PREFETCH)
                             .prefetcher<CACHE::pprefetcherDno>()
                             .replacement<CACHE::rreplacementDlru>();

const auto default_ptw =
    PageTableWalker::Builder{}.tag_bandwidth(2).fill_bandwidth(2).mshr_size(5).add_pscl(5, 1, 2).add_pscl(4, 1, 4).add_pscl(3, 2, 4).add_pscl(2, 4, 8);
} // namespace champsim::defaults

#endif
