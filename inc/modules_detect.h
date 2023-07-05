#ifndef MODULES_DETECT_H
#define MODULES_DETECT_H

#include "block.h"
#include "util/detect.h"

#include <type_traits>

namespace champsim::modules::detect
{
namespace branch_predictor
{
  namespace detail {
    template<typename T, typename... Args>
      static auto initialize_member(int) -> decltype(std::declval<T>().initialize_branch_predictor(std::declval<Args>()...), std::true_type{});
    template<typename, typename...>
      static auto initialize_member(long) -> std::false_type;

    template<typename T, typename... Args>
      static auto last_branch_result_member(int) -> decltype(std::declval<T>().last_branch_result(std::declval<Args>()...), std::true_type{});
    template<typename, typename...>
      static auto last_branch_result_member(long) -> std::false_type;

    template<typename T, typename... Args>
      static auto predict_branch_member(int) -> decltype(std::declval<T>().predict_branch(std::declval<Args>()...), std::true_type{});
    template<typename, typename...>
      static auto predict_branch_member(long) -> std::false_type;
  }

template<typename T, typename... Args>
constexpr bool has_initialize = decltype(detail::initialize_member<T, Args...>(0))::value;

template<typename T, typename... Args>
constexpr bool has_last_branch_result = decltype(detail::last_branch_result_member<T, Args...>(0))::value;

template<typename T, typename... Args>
constexpr bool has_predict_branch = decltype(detail::predict_branch_member<T, Args...>(0))::value;
}

namespace btb
{
  namespace detail {
    template<typename T, typename... Args>
      static auto initialize_member(int) -> decltype(std::declval<T>().initialize_btb(std::declval<Args>()...), std::true_type{});
    template<typename, typename...>
      static auto initialize_member(long) -> std::false_type;

    template<typename T, typename... Args>
      static auto update_member(int) -> decltype(std::declval<T>().update_btb(std::declval<Args>()...), std::true_type{});
    template<typename, typename...>
      static auto update_member(long) -> std::false_type;

    template<typename T, typename... Args>
      static auto predict_branch_member(int) -> decltype(std::declval<T>().btb_prediction(std::declval<Args>()...), std::true_type{});
    template<typename, typename...>
      static auto predict_branch_member(long) -> std::false_type;
  }

template<typename T, typename... Args>
constexpr bool has_initialize = decltype(detail::initialize_member<T, Args...>(0))::value;

template<typename T, typename... Args>
constexpr bool has_update_btb = decltype(detail::update_member<T, Args...>(0))::value;

template<typename T, typename... Args>
constexpr bool has_btb_prediction = decltype(detail::predict_branch_member<T, Args...>(0))::value;
} // namespace btb

namespace prefetcher
{
  namespace detail {
    template<typename T, typename... Args>
      static auto initialize_member(int) -> decltype(std::declval<T>().prefetcher_initialize(std::declval<Args>()...), std::true_type{});
    template<typename, typename...>
      static auto initialize_member(long) -> std::false_type;

    template<typename T, typename... Args>
      static auto cache_operate_member(int) -> decltype(std::declval<T>().prefetcher_cache_operate(std::declval<Args>()...), std::true_type{});
    template<typename, typename...>
      static auto cache_operate_member(long) -> std::false_type;

    template<typename T, typename... Args>
      static auto cache_fill_member(int) -> decltype(std::declval<T>().prefetcher_cache_fill(std::declval<Args>()...), std::true_type{});
    template<typename, typename...>
      static auto cache_fill_member(long) -> std::false_type;

    template<typename T, typename... Args>
      static auto cycle_operate_member(int) -> decltype(std::declval<T>().prefetcher_cycle_operate(std::declval<Args>()...), std::true_type{});
    template<typename, typename...>
      static auto cycle_operate_member(long) -> std::false_type;

    template<typename T, typename... Args>
      static auto final_stats_member(int) -> decltype(std::declval<T>().prefetcher_final_stats(std::declval<Args>()...), std::true_type{});
    template<typename, typename...>
      static auto final_stats_member(long) -> std::false_type;

    template<typename T, typename... Args>
      static auto branch_operate_member(int) -> decltype(std::declval<T>().prefetcher_branch_operate(std::declval<Args>()...), std::true_type{});
    template<typename, typename...>
      static auto branch_operate_member(long) -> std::false_type;
  }

template<typename T, typename... Args>
constexpr bool has_initialize = decltype(detail::initialize_member<T, Args...>(0))::value;

template<typename T, typename... Args>
constexpr bool has_cache_operate = decltype(detail::cache_operate_member<T, Args...>(0))::value;

template<typename T, typename... Args>
constexpr bool has_cache_fill = decltype(detail::cache_fill_member<T, Args...>(0))::value;

template<typename T, typename... Args>
constexpr bool has_cycle_operate = decltype(detail::cycle_operate_member<T, Args...>(0))::value;

template<typename T, typename... Args>
constexpr bool has_final_stats = decltype(detail::final_stats_member<T, Args...>(0))::value;

template<typename T, typename... Args>
constexpr bool has_branch_operate = decltype(detail::branch_operate_member<T, Args...>(0))::value;
} // namespace prefetcher

namespace replacement
{
  namespace detail {
    template<typename T, typename... Args>
      static auto initialize_member(int) -> decltype(std::declval<T>().initialize_replacement(std::declval<Args>()...), std::true_type{});
    template<typename, typename...>
      static auto initialize_member(long) -> std::false_type;

    template<typename T, typename... Args>
      static auto find_victim_member(int) -> decltype(std::declval<T>().find_victim(std::declval<Args>()...), std::true_type{});
    template<typename, typename...>
      static auto find_victim_member(long) -> std::false_type;

    template<typename T, typename... Args>
      static auto update_state_member(int) -> decltype(std::declval<T>().update_replacement_state(std::declval<Args>()...), std::true_type{});
    template<typename, typename...>
      static auto update_state_member(long) -> std::false_type;

    template<typename T, typename... Args>
      static auto final_stats_member(int) -> decltype(std::declval<T>().replacement_final_stats(std::declval<Args>()...), std::true_type{});
    template<typename, typename...>
      static auto final_stats_member(long) -> std::false_type;
  }

template<typename T, typename... Args>
constexpr bool has_initialize = decltype(detail::initialize_member<T, Args...>(0))::value;

template<typename T, typename... Args>
constexpr bool has_find_victim = decltype(detail::find_victim_member<T, Args...>(0))::value;

template<typename T, typename... Args>
constexpr bool has_update_state = decltype(detail::update_state_member<T, Args...>(0))::value;

template<typename T, typename... Args>
constexpr bool has_final_stats = decltype(detail::final_stats_member<T, Args...>(0))::value;
} // namespace replacement
} // namespace champsim::modules::detect

#endif
