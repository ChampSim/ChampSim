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

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "modules.h"

namespace champsim::defaults
{
// O3_CPU parameters - types must exactly match get_parameter<T>() calls in ooo_cpu.h
inline champsim::modules::ModuleBuilder default_core()
{
  return champsim::modules::ModuleBuilder{}
      .add_parameter("clock_period", champsim::chrono::picoseconds{250})
      .add_parameter("dib_set", uint32_t{32})
      .add_parameter("dib_way", uint32_t{8})
      .add_parameter("dib_window", std::size_t{16})
      .add_parameter("ifetch_buffer_size", uint32_t{64})
      .add_parameter("decode_buffer_size", uint32_t{32})
      .add_parameter("dispatch_buffer_size", uint32_t{32})
      .add_parameter("dib_hit_buffer_size", uint32_t{32})
      .add_parameter("register_file_size", uint32_t{128})
      .add_parameter("rob_size", uint32_t{352})
      .add_parameter("lq_size", uint32_t{128})
      .add_parameter("sq_size", uint32_t{72})
      .add_parameter("fetch_width", champsim::bandwidth::maximum_type{6})
      .add_parameter("decode_width", champsim::bandwidth::maximum_type{6})
      .add_parameter("dispatch_width", champsim::bandwidth::maximum_type{6})
      .add_parameter("execute_width", champsim::bandwidth::maximum_type{4})
      .add_parameter("lq_width", champsim::bandwidth::maximum_type{2})
      .add_parameter("sq_width", champsim::bandwidth::maximum_type{2})
      .add_parameter("retire_width", champsim::bandwidth::maximum_type{5})
      .add_parameter("dib_inorder_width", champsim::bandwidth::maximum_type{5})
      .add_parameter("mispredict_penalty", unsigned{1})
      .add_parameter("schedule_width", champsim::bandwidth::maximum_type{128})
      .add_parameter("decode_latency", unsigned{1})
      .add_parameter("dib_hit_latency", unsigned{1})
      .add_parameter("dispatch_latency", unsigned{1})
      .add_parameter("schedule_latency", unsigned{0})
      .add_parameter("execute_latency", unsigned{0})
      .add_parameter("l1i_bandwidth", champsim::bandwidth::maximum_type{1})
      .add_parameter("l1d_bandwidth", champsim::bandwidth::maximum_type{1})
      .add_parameter("fetch_queues", static_cast<champsim::modules::channel_module*>(nullptr))
      .add_parameter("data_queues", static_cast<champsim::modules::channel_module*>(nullptr))
      .add_parameter("l1i", static_cast<champsim::modules::cache_module*>(nullptr))
      .add_submodule("branch_predictor", champsim::modules::ModuleBuilder{"default_bp", "bimodal"})
      .add_submodule("btb", champsim::modules::ModuleBuilder{"default_btb", "basic_btb"});
}

// CACHE parameters - types must exactly match get_parameter<T>() calls in cache.h
inline champsim::modules::ModuleBuilder default_l1i()
{
  return champsim::modules::ModuleBuilder{}
      .add_parameter("clock_period", champsim::chrono::picoseconds{250})
      .add_parameter("num_sets", uint32_t{64})
      .add_parameter("num_ways", uint32_t{8})
      .add_parameter("pq_size", std::size_t{32})
      .add_parameter("hit_latency", uint64_t{2})
      .add_parameter("fill_latency", uint64_t{2})
      .add_parameter("offset_bits", champsim::data::bits{6})
      .add_parameter("max_tag_bandwidth", champsim::bandwidth::maximum_type{1})
      .add_parameter("max_fill_bandwidth", champsim::bandwidth::maximum_type{1})
      .add_parameter("prefetch_as_load", false)
      .add_parameter("match_offset_bits", true)
      .add_parameter("virtual_prefetch", true)
      .add_parameter("pref_activate_mask", std::vector<access_type>{access_type::LOAD, access_type::PREFETCH})
      .add_submodule("prefetcher", champsim::modules::ModuleBuilder{"default_pref", "no"})
      .add_submodule("replacement", champsim::modules::ModuleBuilder{"default_repl", "lru"})
      .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{})
      .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(nullptr))
      .add_parameter("lower_translate", static_cast<champsim::modules::channel_module*>(nullptr));
}

inline champsim::modules::ModuleBuilder default_l1d()
{
  return champsim::modules::ModuleBuilder{}
      .add_parameter("clock_period", champsim::chrono::picoseconds{250})
      .add_parameter("num_sets", uint32_t{64})
      .add_parameter("num_ways", uint32_t{12})
      .add_parameter("pq_size", std::size_t{8})
      .add_parameter("hit_latency", uint64_t{2})
      .add_parameter("fill_latency", uint64_t{2})
      .add_parameter("offset_bits", champsim::data::bits{6})
      .add_parameter("max_tag_bandwidth", champsim::bandwidth::maximum_type{1})
      .add_parameter("max_fill_bandwidth", champsim::bandwidth::maximum_type{1})
      .add_parameter("prefetch_as_load", false)
      .add_parameter("match_offset_bits", true)
      .add_parameter("virtual_prefetch", false)
      .add_parameter("pref_activate_mask", std::vector<access_type>{access_type::LOAD, access_type::PREFETCH})
      .add_submodule("prefetcher", champsim::modules::ModuleBuilder{"default_pref", "no"})
      .add_submodule("replacement", champsim::modules::ModuleBuilder{"default_repl", "lru"})
      .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{})
      .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(nullptr))
      .add_parameter("lower_translate", static_cast<champsim::modules::channel_module*>(nullptr));
}

inline champsim::modules::ModuleBuilder default_l2c()
{
  return champsim::modules::ModuleBuilder{}
      .add_parameter("clock_period", champsim::chrono::picoseconds{250})
      .add_parameter("num_sets", uint32_t{512})
      .add_parameter("num_ways", uint32_t{8})
      .add_parameter("pq_size", std::size_t{16})
      .add_parameter("hit_latency", uint64_t{4})
      .add_parameter("fill_latency", uint64_t{4})
      .add_parameter("offset_bits", champsim::data::bits{6})
      .add_parameter("max_tag_bandwidth", champsim::bandwidth::maximum_type{1})
      .add_parameter("max_fill_bandwidth", champsim::bandwidth::maximum_type{1})
      .add_parameter("prefetch_as_load", false)
      .add_parameter("match_offset_bits", false)
      .add_parameter("virtual_prefetch", false)
      .add_parameter("pref_activate_mask", std::vector<access_type>{access_type::LOAD, access_type::PREFETCH})
      .add_submodule("prefetcher", champsim::modules::ModuleBuilder{"default_pref", "no"})
      .add_submodule("replacement", champsim::modules::ModuleBuilder{"default_repl", "lru"})
      .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{})
      .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(nullptr))
      .add_parameter("lower_translate", static_cast<champsim::modules::channel_module*>(nullptr));
}

inline champsim::modules::ModuleBuilder default_itlb()
{
  return champsim::modules::ModuleBuilder{}
      .add_parameter("clock_period", champsim::chrono::picoseconds{250})
      .add_parameter("num_sets", uint32_t{16})
      .add_parameter("num_ways", uint32_t{4})
      .add_parameter("pq_size", std::size_t{0})
      .add_parameter("hit_latency", uint64_t{1})
      .add_parameter("fill_latency", uint64_t{1})
      .add_parameter("offset_bits", champsim::data::bits{12})
      .add_parameter("max_tag_bandwidth", champsim::bandwidth::maximum_type{1})
      .add_parameter("max_fill_bandwidth", champsim::bandwidth::maximum_type{1})
      .add_parameter("prefetch_as_load", false)
      .add_parameter("match_offset_bits", true)
      .add_parameter("virtual_prefetch", true)
      .add_parameter("pref_activate_mask", std::vector<access_type>{access_type::LOAD, access_type::PREFETCH})
      .add_submodule("prefetcher", champsim::modules::ModuleBuilder{"default_pref", "no"})
      .add_submodule("replacement", champsim::modules::ModuleBuilder{"default_repl", "lru"})
      .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{})
      .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(nullptr))
      .add_parameter("lower_translate", static_cast<champsim::modules::channel_module*>(nullptr));
}

inline champsim::modules::ModuleBuilder default_dtlb()
{
  return champsim::modules::ModuleBuilder{}
      .add_parameter("clock_period", champsim::chrono::picoseconds{250})
      .add_parameter("num_sets", uint32_t{16})
      .add_parameter("num_ways", uint32_t{4})
      .add_parameter("pq_size", std::size_t{0})
      .add_parameter("mshr_size", uint32_t{8})
      .add_parameter("hit_latency", uint64_t{1})
      .add_parameter("fill_latency", uint64_t{1})
      .add_parameter("offset_bits", champsim::data::bits{12})
      .add_parameter("max_tag_bandwidth", champsim::bandwidth::maximum_type{1})
      .add_parameter("max_fill_bandwidth", champsim::bandwidth::maximum_type{1})
      .add_parameter("prefetch_as_load", false)
      .add_parameter("match_offset_bits", true)
      .add_parameter("virtual_prefetch", false)
      .add_parameter("pref_activate_mask", std::vector<access_type>{access_type::LOAD, access_type::PREFETCH})
      .add_submodule("prefetcher", champsim::modules::ModuleBuilder{"default_pref", "no"})
      .add_submodule("replacement", champsim::modules::ModuleBuilder{"default_repl", "lru"})
      .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{})
      .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(nullptr))
      .add_parameter("lower_translate", static_cast<champsim::modules::channel_module*>(nullptr));
}

inline champsim::modules::ModuleBuilder default_stlb()
{
  return champsim::modules::ModuleBuilder{}
      .add_parameter("clock_period", champsim::chrono::picoseconds{250})
      .add_parameter("num_sets", uint32_t{64})
      .add_parameter("num_ways", uint32_t{12})
      .add_parameter("pq_size", std::size_t{0})
      .add_parameter("hit_latency", uint64_t{2})
      .add_parameter("fill_latency", uint64_t{2})
      .add_parameter("offset_bits", champsim::data::bits{12})
      .add_parameter("max_tag_bandwidth", champsim::bandwidth::maximum_type{1})
      .add_parameter("max_fill_bandwidth", champsim::bandwidth::maximum_type{1})
      .add_parameter("prefetch_as_load", false)
      .add_parameter("match_offset_bits", false)
      .add_parameter("virtual_prefetch", false)
      .add_parameter("pref_activate_mask", std::vector<access_type>{access_type::LOAD, access_type::PREFETCH})
      .add_submodule("prefetcher", champsim::modules::ModuleBuilder{"default_pref", "no"})
      .add_submodule("replacement", champsim::modules::ModuleBuilder{"default_repl", "lru"})
      .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{})
      .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(nullptr))
      .add_parameter("lower_translate", static_cast<champsim::modules::channel_module*>(nullptr));
}

inline champsim::modules::ModuleBuilder default_llc()
{
  return champsim::modules::ModuleBuilder{}
      .add_parameter("clock_period", champsim::chrono::picoseconds{250})
      .add_parameter("num_sets", uint32_t{2048})
      .add_parameter("num_ways", uint32_t{16})
      .add_parameter("pq_size", std::size_t{32})
      .add_parameter("hit_latency", uint64_t{10})
      .add_parameter("fill_latency", uint64_t{10})
      .add_parameter("offset_bits", champsim::data::bits{6})
      .add_parameter("max_tag_bandwidth", champsim::bandwidth::maximum_type{1})
      .add_parameter("max_fill_bandwidth", champsim::bandwidth::maximum_type{1})
      .add_parameter("prefetch_as_load", false)
      .add_parameter("match_offset_bits", false)
      .add_parameter("virtual_prefetch", false)
      .add_parameter("pref_activate_mask", std::vector<access_type>{access_type::LOAD, access_type::PREFETCH})
      .add_submodule("prefetcher", champsim::modules::ModuleBuilder{"default_pref", "no"})
      .add_submodule("replacement", champsim::modules::ModuleBuilder{"default_repl", "lru"})
      .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{})
      .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(nullptr))
      .add_parameter("lower_translate", static_cast<champsim::modules::channel_module*>(nullptr));
}

// PageTableWalker parameters - types must match get_parameter<T>() calls in ptw.cc
inline champsim::modules::ModuleBuilder default_ptw()
{
  return champsim::modules::ModuleBuilder{}
      .add_parameter("clock_period", champsim::chrono::picoseconds{250})
      .add_parameter("mshr_size", uint32_t{5})
      .add_parameter("max_tag_check", champsim::bandwidth::maximum_type{2})
      .add_parameter("max_fill", champsim::bandwidth::maximum_type{2})
      .add_parameter("latency", unsigned{0})
      .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{})
      .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(nullptr))
      .add_parameter("lower_translate", static_cast<champsim::modules::channel_module*>(nullptr))
      .add_parameter("vmem", static_cast<champsim::modules::vmem_module*>(nullptr))
      .add_parameter("pscl_dims", std::vector<std::array<uint32_t, 3>>{{5, 1, 2}, {4, 1, 4}, {3, 2, 4}, {2, 4, 8}});
}

// champsim::channel parameters - types must match get_parameter<T>() calls in channel.cc
inline champsim::modules::ModuleBuilder default_channel()
{
  return champsim::modules::ModuleBuilder{}
      .add_parameter("rq_size", std::size_t{32})
      .add_parameter("pq_size", std::size_t{32})
      .add_parameter("wq_size", std::size_t{32})
      .add_parameter("offset_bits", champsim::data::bits{0})
      .add_parameter("match_offset_bits", false);
}

// MEMORY_CONTROLLER parameters - types must match get_parameter<T>() calls in dram_controller.cc
inline champsim::modules::ModuleBuilder default_memory_controller()
{
  return champsim::modules::ModuleBuilder{}
      .add_parameter("dbus_period", champsim::chrono::picoseconds{3200})
      .add_parameter("mc_period", champsim::chrono::picoseconds{6400})
      .add_parameter("n_rp", std::size_t{18})
      .add_parameter("n_rcd", std::size_t{18})
      .add_parameter("n_cas", std::size_t{18})
      .add_parameter("n_ras", std::size_t{38})
      .add_parameter("refresh_period", champsim::chrono::microseconds{64000})
      .add_parameter("ul_channels", std::vector<champsim::modules::channel_module*>{})
      .add_parameter("rq_size", std::size_t{64})
      .add_parameter("wq_size", std::size_t{64})
      .add_parameter("channels", std::size_t{1})
      .add_parameter("channel_width", champsim::data::bytes{8})
      .add_parameter("rows", std::size_t{1024})
      .add_parameter("columns", std::size_t{1024})
      .add_parameter("ranks", std::size_t{4})
      .add_parameter("bankgroups", std::size_t{4})
      .add_parameter("banks", std::size_t{4})
      .add_parameter("refreshes_per_period", std::size_t{8192});
}

// VirtualMemory parameters - types must match get_parameter<T>() calls in vmem.cc
inline champsim::modules::ModuleBuilder default_vmem()
{
  return champsim::modules::ModuleBuilder{}
      .add_parameter("page_table_page_size", champsim::data::bytes{4096})
      .add_parameter("page_table_levels", std::size_t{4})
      .add_parameter("minor_fault_penalty", champsim::chrono::picoseconds{6400000}) // 6400ns in picoseconds
      .add_parameter("randomization_seed", std::optional<uint64_t>{});
}
} // namespace champsim::defaults

#endif
