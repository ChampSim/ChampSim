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

#ifndef MODULES_H
#define MODULES_H

#include <cstdint>
#include <type_traits>
#include <utility>

#include "access_type.h"
#include "address.h"
#include "block.h"
#include "champsim.h"

class CACHE;
class O3_CPU;
namespace champsim::modules
{
inline constexpr bool warn_if_any_missing = true;
template <typename T>
[[deprecated]] void does_not_have()
{
}

template <typename T>
struct bound_to {
  T* intern_;
  explicit bound_to(T* bind_arg) { bind(bind_arg); }
  void bind(T* bind_arg) { intern_ = bind_arg; }
};

struct branch_predictor : public bound_to<O3_CPU> {
  explicit branch_predictor(O3_CPU* cpu) : bound_to<O3_CPU>(cpu) {}

  template <typename T, typename... Args>
  static auto initialize_member_impl(int) -> decltype(std::declval<T>().initialize_branch_predictor(std::declval<Args>()...), std::true_type{});
  template <typename, typename...>
  static auto initialize_member_impl(long) -> std::false_type;

  template <typename T, typename... Args>
  static auto last_branch_result_member_impl(int) -> decltype(std::declval<T>().last_branch_result(std::declval<Args>()...), std::true_type{});
  template <typename, typename...>
  static auto last_branch_result_member_impl(long) -> std::false_type;

  template <typename T, typename... Args>
  static auto predict_branch_member_impl(int) -> decltype(std::declval<T>().predict_branch(std::declval<Args>()...), std::true_type{});
  template <typename, typename...>
  static auto predict_branch_member_impl(long) -> std::false_type;

  template <typename T, typename... Args>
  constexpr static bool has_initialize = decltype(initialize_member_impl<T, Args...>(0))::value;

  template <typename T, typename... Args>
  constexpr static bool has_last_branch_result = decltype(last_branch_result_member_impl<T, Args...>(0))::value;

  template <typename T, typename... Args>
  constexpr static bool has_predict_branch = decltype(predict_branch_member_impl<T, Args...>(0))::value;
};

struct btb : public bound_to<O3_CPU> {
  explicit btb(O3_CPU* cpu) : bound_to<O3_CPU>(cpu) {}

  template <typename T, typename... Args>
  static auto initialize_member_impl(int) -> decltype(std::declval<T>().initialize_btb(std::declval<Args>()...), std::true_type{});
  template <typename, typename...>
  static auto initialize_member_impl(long) -> std::false_type;

  template <typename T, typename... Args>
  static auto update_member_impl(int) -> decltype(std::declval<T>().update_btb(std::declval<Args>()...), std::true_type{});
  template <typename, typename...>
  static auto update_member_impl(long) -> std::false_type;

  template <typename T, typename... Args>
  static auto predict_branch_member_impl(int) -> decltype(std::declval<T>().btb_prediction(std::declval<Args>()...), std::true_type{});
  template <typename, typename...>
  static auto predict_branch_member_impl(long) -> std::false_type;

  template <typename T, typename... Args>
  constexpr static bool has_initialize = decltype(initialize_member_impl<T, Args...>(0))::value;

  template <typename T, typename... Args>
  constexpr static bool has_update_btb = decltype(update_member_impl<T, Args...>(0))::value;

  template <typename T, typename... Args>
  constexpr static bool has_btb_prediction = decltype(predict_branch_member_impl<T, Args...>(0))::value;
};

struct prefetcher : public bound_to<CACHE> {
  explicit prefetcher(CACHE* cache) : bound_to<CACHE>(cache) {}
  bool prefetch_line(champsim::address pf_addr, bool fill_this_level, uint32_t prefetch_metadata) const;
  [[deprecated]] bool prefetch_line(uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata) const;

  template <typename T, typename... Args>
  static auto initiailize_memory_impl(int) -> decltype(std::declval<T>().prefetcher_initialize(std::declval<Args>()...), std::true_type{});
  template <typename, typename...>
  static auto initiailize_memory_impl(long) -> std::false_type;

  template <typename T, typename... Args>
  static auto cache_operate_member_impl(int) -> decltype(std::declval<T>().prefetcher_cache_operate(std::declval<Args>()...), std::true_type{});
  template <typename, typename...>
  static auto cache_operate_member_impl(long) -> std::false_type;

  template <typename T, typename... Args>
  static auto cache_fill_member_impl(int) -> decltype(std::declval<T>().prefetcher_cache_fill(std::declval<Args>()...), std::true_type{});
  template <typename, typename...>
  static auto cache_fill_member_impl(long) -> std::false_type;

  template <typename T, typename... Args>
  static auto cycle_operate_member_impl(int) -> decltype(std::declval<T>().prefetcher_cycle_operate(std::declval<Args>()...), std::true_type{});
  template <typename, typename...>
  static auto cycle_operate_member_impl(long) -> std::false_type;

  template <typename T, typename... Args>
  static auto final_stats_member_impl(int) -> decltype(std::declval<T>().prefetcher_final_stats(std::declval<Args>()...), std::true_type{});
  template <typename, typename...>
  static auto final_stats_member_impl(long) -> std::false_type;

  template <typename T, typename... Args>
  static auto branch_operate_member_impl(int) -> decltype(std::declval<T>().prefetcher_branch_operate(std::declval<Args>()...), std::true_type{});
  template <typename, typename...>
  static auto branch_operate_member_impl(long) -> std::false_type;

  template <typename T, typename... Args>
  constexpr static bool has_initialize = decltype(initiailize_memory_impl<T, Args...>(0))::value;

  template <typename T, typename... Args>
  constexpr static bool has_cache_operate = decltype(cache_operate_member_impl<T, Args...>(0))::value;

  template <typename T, typename... Args>
  constexpr static bool has_cache_fill = decltype(cache_fill_member_impl<T, Args...>(0))::value;

  template <typename T, typename... Args>
  constexpr static bool has_cycle_operate = decltype(cycle_operate_member_impl<T, Args...>(0))::value;

  template <typename T, typename... Args>
  constexpr static bool has_final_stats = decltype(final_stats_member_impl<T, Args...>(0))::value;

  template <typename T, typename... Args>
  constexpr static bool has_branch_operate = decltype(branch_operate_member_impl<T, Args...>(0))::value;
};

struct replacement : public bound_to<CACHE> {
  explicit replacement(CACHE* cache) : bound_to<CACHE>(cache) {}

  template <typename T, typename... Args>
  static auto initialize_member_impl(int) -> decltype(std::declval<T>().initialize_replacement(std::declval<Args>()...), std::true_type{});
  template <typename, typename...>
  static auto initialize_member_impl(long) -> std::false_type;

  template <typename T, typename... Args>
  static auto find_victim_member_impl(int) -> decltype(std::declval<T>().find_victim(std::declval<Args>()...), std::true_type{});
  template <typename, typename...>
  static auto find_victim_member_impl(long) -> std::false_type;

  template <typename T, typename... Args>
  static auto update_state_member_impl(int) -> decltype(std::declval<T>().update_replacement_state(std::declval<Args>()...), std::true_type{});
  template <typename, typename...>
  static auto update_state_member_impl(long) -> std::false_type;

  template <typename T, typename... Args>
  static auto cache_fill_member_impl(int) -> decltype(std::declval<T>().replacement_cache_fill(std::declval<Args>()...), std::true_type{});
  template <typename, typename...>
  static auto cache_fill_member_impl(long) -> std::false_type;

  template <typename T, typename... Args>
  static auto final_stats_member_impl(int) -> decltype(std::declval<T>().replacement_final_stats(std::declval<Args>()...), std::true_type{});
  template <typename, typename...>
  static auto final_stats_member_impl(long) -> std::false_type;

  template <typename T, typename... Args>
  constexpr static bool has_initialize = decltype(initialize_member_impl<T, Args...>(0))::value;

  template <typename T, typename... Args>
  constexpr static bool has_find_victim = decltype(find_victim_member_impl<T, Args...>(0))::value;

  template <typename T, typename... Args>
  constexpr static bool has_update_state = decltype(update_state_member_impl<T, Args...>(0))::value;

  template <typename T, typename... Args>
  constexpr static bool has_cache_fill = decltype(cache_fill_member_impl<T, Args...>(0))::value;

  template <typename T, typename... Args>
  constexpr static bool has_final_stats = decltype(final_stats_member_impl<T, Args...>(0))::value;
};
} // namespace champsim::modules

#endif
