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

#include "../branch/hashed_perceptron/hashed_perceptron.h"
#include "../btb/basic_btb/basic_btb.h"
#include "../prefetcher/no/no.h"
#include "../replacement/lru/lru.h"
#include "cache_builder.h"
#include "core_builder.h"
#include "ptw_builder.h"

namespace champsim::defaults
{
const auto default_core =
    champsim::core_builder<champsim::core_builder_module_type_holder<hashed_perceptron>, champsim::core_builder_module_type_holder<basic_btb>>{}
        .dib_set(32)
        .dib_way(8)
        .dib_window(16)
        .ifetch_buffer_size(64)
        .decode_buffer_size(32)
        .dispatch_buffer_size(32)
        .dib_hit_buffer_size(32) // assumed
        .register_file_size(128)
        .rob_size(352)
        .lq_size(128)
        .sq_size(72)
        .fetch_width(champsim::bandwidth::maximum_type{6})
        .decode_width(champsim::bandwidth::maximum_type{6})
        .dispatch_width(champsim::bandwidth::maximum_type{6})
        .execute_width(champsim::bandwidth::maximum_type{4})
        .lq_width(champsim::bandwidth::maximum_type{2})
        .sq_width(champsim::bandwidth::maximum_type{2})
        .retire_width(champsim::bandwidth::maximum_type{5})
        .dib_inorder_width(champsim::bandwidth::maximum_type{5}) // assumed
        .mispredict_penalty(1)
        .schedule_width(champsim::bandwidth::maximum_type{128})
        .decode_latency(1)
        .dib_hit_latency(1)
        .dispatch_latency(1)
        .schedule_latency(0)
        .execute_latency(0)
        .l1i_bandwidth(champsim::bandwidth::maximum_type{1})
        .l1d_bandwidth(champsim::bandwidth::maximum_type{1});

const auto default_l1i = champsim::cache_builder<champsim::cache_builder_module_type_holder<no>, champsim::cache_builder_module_type_holder<lru>>{}
                             .sets_factor(64)
                             .ways(8)
                             .pq_size(32)
                             .offset_bits(champsim::data::bits{LOG2_BLOCK_SIZE})
                             .reset_prefetch_as_load()
                             .set_virtual_prefetch()
                             .set_wq_checks_full_addr()
                             .prefetch_activate(access_type::LOAD, access_type::PREFETCH);

const auto default_l1d = champsim::cache_builder<champsim::cache_builder_module_type_holder<no>, champsim::cache_builder_module_type_holder<lru>>{}
                             .sets_factor(64)
                             .ways(12)
                             .pq_size(8)
                             .offset_bits(champsim::data::bits{LOG2_BLOCK_SIZE})
                             .reset_prefetch_as_load()
                             .reset_virtual_prefetch()
                             .set_wq_checks_full_addr()
                             .prefetch_activate(access_type::LOAD, access_type::PREFETCH);

const auto default_l2c = champsim::cache_builder<champsim::cache_builder_module_type_holder<no>, champsim::cache_builder_module_type_holder<lru>>{}
                             .sets_factor(512)
                             .ways(8)
                             .pq_size(16)
                             .offset_bits(champsim::data::bits{LOG2_BLOCK_SIZE})
                             .reset_prefetch_as_load()
                             .reset_virtual_prefetch()
                             .reset_wq_checks_full_addr()
                             .prefetch_activate(access_type::LOAD, access_type::PREFETCH);

const auto default_itlb = champsim::cache_builder<champsim::cache_builder_module_type_holder<no>, champsim::cache_builder_module_type_holder<lru>>{}
                              .sets_factor(16)
                              .ways(4)
                              .pq_size(0)
                              .offset_bits(champsim::data::bits{LOG2_PAGE_SIZE})
                              .reset_prefetch_as_load()
                              .set_virtual_prefetch()
                              .set_wq_checks_full_addr()
                              .prefetch_activate(access_type::LOAD, access_type::PREFETCH);

const auto default_dtlb = champsim::cache_builder<champsim::cache_builder_module_type_holder<no>, champsim::cache_builder_module_type_holder<lru>>{}
                              .sets_factor(16)
                              .ways(4)
                              .pq_size(0)
                              .mshr_size(8)
                              .offset_bits(champsim::data::bits{LOG2_PAGE_SIZE})
                              .reset_prefetch_as_load()
                              .reset_virtual_prefetch()
                              .set_wq_checks_full_addr()
                              .prefetch_activate(access_type::LOAD, access_type::PREFETCH);

const auto default_stlb = champsim::cache_builder<champsim::cache_builder_module_type_holder<no>, champsim::cache_builder_module_type_holder<lru>>{}
                              .sets_factor(64)
                              .ways(12)
                              .pq_size(0)
                              .offset_bits(champsim::data::bits{LOG2_PAGE_SIZE})
                              .reset_prefetch_as_load()
                              .reset_virtual_prefetch()
                              .reset_wq_checks_full_addr()
                              .prefetch_activate(access_type::LOAD, access_type::PREFETCH);

const auto default_llc = champsim::cache_builder<champsim::cache_builder_module_type_holder<no>, champsim::cache_builder_module_type_holder<lru>>{}
                             .name("LLC")
                             .sets_factor(2048)
                             .ways(16)
                             .pq_size(32)
                             .offset_bits(champsim::data::bits{LOG2_BLOCK_SIZE})
                             .reset_prefetch_as_load()
                             .reset_virtual_prefetch()
                             .reset_wq_checks_full_addr()
                             .prefetch_activate(access_type::LOAD, access_type::PREFETCH);

const auto default_ptw = champsim::ptw_builder{}.bandwidth_factor(2).mshr_factor(5).add_pscl(5, 1, 2).add_pscl(4, 1, 4).add_pscl(3, 2, 4).add_pscl(2, 4, 8);
} // namespace champsim::defaults

#endif
