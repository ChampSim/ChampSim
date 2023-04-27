#ifndef MODULES_DETECT_H
#define MODULES_DETECT_H

#include "util/detect.h"

namespace champsim::modules::detect
{
  namespace branch_predictor
  {
    namespace detail
    {
      template <typename T>
      using has_initialize = decltype(std::declval<T>().initialize_branch_predictor());

      template <typename T>
      using has_last_branch_result_v1 = decltype(std::declval<T>().last_branch_result(std::declval<uint64_t>(), std::declval<uint64_t>(), std::declval<bool>(), std::declval<uint8_t>()));

      template <typename T>
      using has_last_branch_result_v2 = decltype(std::declval<T>().last_branch_result(std::declval<champsim::address>(), std::declval<champsim::address>(), std::declval<bool>(), std::declval<uint8_t>()));

      template <typename T>
      using has_predict_branch_v1 = decltype(std::declval<T>().predict_branch(std::declval<uint64_t>()));

      template <typename T>
      using has_predict_branch_v2 = decltype(std::declval<T>().predict_branch(std::declval<champsim::address>()));
    }

    template <typename T>
    constexpr auto has_initialize()
    {
      return champsim::is_detected_v<detail::has_initialize, T>;
    }

    template <typename T>
    constexpr auto has_last_branch_result()
    {
      if (champsim::is_detected_v<detail::has_last_branch_result_v1, T>)
        return 1;
      if (champsim::is_detected_v<detail::has_last_branch_result_v2, T>)
        return 2;
      return 0;
    }

    template <typename T>
    constexpr auto has_predict_branch()
    {
      if (champsim::is_detected_v<detail::has_predict_branch_v1, T>)
        return 1;
      if (champsim::is_detected_v<detail::has_predict_branch_v2, T>)
        return 2;
      return 0;
    }
  }

  namespace btb
  {
    namespace detail
    {
      template <typename T>
      using has_initialize = decltype(std::declval<T>().initialize_btb());

      template <typename T>
      using has_update_btb_v1 = decltype(std::declval<T>().update_btb(std::declval<uint64_t>(), std::declval<uint64_t>(), std::declval<bool>(), std::declval<uint8_t>()));

      template <typename T>
      using has_update_btb_v2 = decltype(std::declval<T>().update_btb(std::declval<champsim::address>(), std::declval<champsim::address>(), std::declval<bool>(), std::declval<uint8_t>()));

      template <typename T>
      using has_btb_prediction_v1 = decltype(std::declval<T>().btb_prediction(std::declval<uint64_t>()));

      template <typename T>
      using has_btb_prediction_v2 = decltype(std::declval<T>().btb_prediction(std::declval<champsim::address>()));
    }

    template <typename T>
    constexpr auto has_initialize()
    {
      return champsim::is_detected_v<detail::has_initialize, T>;
    }

    template <typename T>
    constexpr auto has_update_btb()
    {
      if (champsim::is_detected_v<detail::has_update_btb_v1, T>)
        return 1;
      if (champsim::is_detected_v<detail::has_update_btb_v2, T>)
        return 2;
      return 0;
    }

    template <typename T>
    constexpr auto has_btb_prediction()
    {
      if (champsim::is_detected_v<detail::has_btb_prediction_v1, T>)
        return 1;
      if (champsim::is_detected_v<detail::has_btb_prediction_v2, T>)
        return 2;
      return 0;
    }
  }

  namespace prefetcher
  {
    namespace detail
    {
      template <typename T>
      using has_initialize = decltype(std::declval<T>().prefetcher_initialize());

      template <typename T>
      using has_cache_operate_v1 = decltype( std::declval<T>().prefetcher_cache_operate(std::declval<uint64_t>(), std::declval<uint64_t>(), std::declval<uint8_t>(), std::declval<uint8_t>(), std::declval<uint32_t>() ) );

      template <typename T>
      using has_cache_operate_v2 = decltype( std::declval<T>().prefetcher_cache_operate(std::declval<champsim::address>(), std::declval<champsim::address>(), std::declval<uint8_t>(), std::declval<uint8_t>(), std::declval<uint32_t>() ) );

      template <typename T>
      using has_cache_fill_v1 = decltype( std::declval<T>().prefetcher_cache_fill(std::declval<uint64_t>(), std::declval<long>(), std::declval<long>(), std::declval<uint8_t>(), std::declval<uint64_t>(), std::declval<uint32_t>() ) );

      template <typename T>
      using has_cache_fill_v2 = decltype( std::declval<T>().prefetcher_cache_fill(std::declval<champsim::address>(), std::declval<long>(), std::declval<long>(), std::declval<uint8_t>(), std::declval<champsim::address>(), std::declval<uint32_t>() ) );

      template <typename T>
      using has_cycle_operate = decltype(std::declval<T>().prefetcher_cycle_operate());

      template <typename T>
      using has_final_stats = decltype(std::declval<T>().prefetcher_final_stats());

      template <typename T>
      using has_branch_operate_v1 = decltype( std::declval<T>().prefetcher_branch_operate(std::declval<uint64_t>(), std::declval<uint8_t>(), std::declval<uint64_t>()) );

      template <typename T>
      using has_branch_operate_v2 = decltype( std::declval<T>().prefetcher_branch_operate(std::declval<champsim::address>(), std::declval<uint8_t>(), std::declval<champsim::address>()) );
    }

    template <typename T>
    constexpr auto has_initialize()
    {
      return champsim::is_detected_v<detail::has_initialize, T>;
    }

    template <typename T>
    constexpr auto has_cache_operate()
    {
      if (champsim::is_detected_v<detail::has_cache_operate_v1, T>)
        return 1;
      if (champsim::is_detected_v<detail::has_cache_operate_v2, T>)
        return 2;
      return 0;
    }

    template <typename T>
    constexpr auto has_cache_fill()
    {
      if (champsim::is_detected_v<detail::has_cache_fill_v1, T>)
        return 1;
      if (champsim::is_detected_v<detail::has_cache_fill_v2, T>)
        return 2;
      return 0;
    }

    template <typename T>
    constexpr auto has_cycle_operate()
    {
      return champsim::is_detected_v<detail::has_cycle_operate, T>;
    }

    template <typename T>
    constexpr auto has_final_stats()
    {
      return champsim::is_detected_v<detail::has_final_stats, T>;
    }

    template <typename T>
    constexpr auto has_branch_operate()
    {
      if (champsim::is_detected_v<detail::has_branch_operate_v1, T>)
        return 1;
      if (champsim::is_detected_v<detail::has_branch_operate_v2, T>)
        return 2;
      return 0;
    }
  }

  namespace replacement
  {
    namespace detail
    {
      template <typename T>
      using has_initialize = decltype(std::declval<T>().initialize_replacement());

      template <typename T>
      using has_find_victim_v1 = decltype( std::declval<T>().find_victim(std::declval<uint32_t>(), std::declval<uint64_t>(), std::declval<long>(), std::declval<const BLOCK*>(), std::declval<uint64_t>(), std::declval<uint64_t>(), std::declval<uint32_t>()));

      template <typename T>
      using has_find_victim_v2 = decltype( std::declval<T>().find_victim(std::declval<uint32_t>(), std::declval<uint64_t>(), std::declval<long>(), std::declval<const BLOCK*>(), std::declval<champsim::address>(), std::declval<champsim::address>(), std::declval<uint32_t>()));

      template <typename T>
      using has_update_state_v1 = decltype( std::declval<T>().update_replacement_state(std::declval<uint32_t>(), std::declval<long>(), std::declval<long>(), std::declval<uint64_t>(), std::declval<uint64_t>(), std::declval<uint64_t>(), std::declval<uint32_t>(), std::declval<uint8_t>()));

      template <typename T>
      using has_update_state_v2 = decltype( std::declval<T>().update_replacement_state(std::declval<uint32_t>(), std::declval<long>(), std::declval<long>(), std::declval<champsim::address>(), std::declval<champsim::address>(), std::declval<champsim::address>(), std::declval<uint32_t>(), std::declval<uint8_t>()));

      template <typename T>
      using has_final_stats = decltype(std::declval<T>().replacement_final_stats());
    }

    template <typename T>
    constexpr auto has_initialize()
    {
      if (champsim::is_detected_v<detail::has_initialize, T>)
        return true;
      // else if (champsim::modules::warn_if_any_missing)
      //   champsim::modules::does_not_have<decltype(r)>();
      // return false;
    }

    template <typename T>
    constexpr auto has_find_victim()
    {
      if (champsim::is_detected_v<detail::has_find_victim_v1, T>)
        return 1;
      if (champsim::is_detected_v<detail::has_find_victim_v2, T>)
        return 2;
      return 0;
    }

    template <typename T>
    constexpr auto has_update_state()
    {
      if (champsim::is_detected_v<detail::has_update_state_v1, T>)
        return 1;
      if (champsim::is_detected_v<detail::has_update_state_v2, T>)
        return 2;
      return 0;
    }

    template <typename T>
    constexpr auto has_final_stats()
    {
      return champsim::is_detected_v<detail::has_final_stats, T>;
    }
  }
}

#endif
