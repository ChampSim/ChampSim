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
    }

    template <typename T>
    constexpr bool has_initialize()
    {
      return champsim::is_detected_v<detail::has_initialize, T>;
    }
  }

  namespace btb
  {
    namespace detail
    {
      template <typename T>
      using has_initialize = decltype(std::declval<T>().initialize_btb());
    }

    template <typename T>
    constexpr bool has_initialize()
    {
      return champsim::is_detected_v<detail::has_initialize, T>;
    }
  }

  namespace prefetcher
  {
    namespace detail
    {
      template <typename T>
      using has_initialize = decltype(std::declval<T>().prefetcher_initialize());

      template <typename T>
      using has_cycle_operate = decltype(std::declval<T>().prefetcher_cycle_operate());

      template <typename T>
      using has_final_stats = decltype(std::declval<T>().prefetcher_final_stats());

      template <typename T>
      using has_branch_operate = decltype( std::declval<T>().prefetcher_branch_operate(std::declval<uint64_t>(), std::declval<uint8_t>(), std::declval<uint64_t>()) );
    }

    template <typename T>
    constexpr bool has_initialize()
    {
      return champsim::is_detected_v<detail::has_initialize, T>;
    }

    template <typename T>
    constexpr bool has_cycle_operate()
    {
      return champsim::is_detected_v<detail::has_cycle_operate, T>;
    }

    template <typename T>
    constexpr bool has_final_stats()
    {
      return champsim::is_detected_v<detail::has_final_stats, T>;
    }

    template <typename T>
    constexpr bool has_branch_operate()
    {
      return champsim::is_detected_v<detail::has_branch_operate, T>;
    }
  }

  namespace replacement
  {
    namespace detail
    {
      template <typename T>
      using has_initialize = decltype(std::declval<T>().initialize_replacement());

      template <typename T>
      using has_update_state = decltype( std::declval<T>().update_replacement_state(std::declval<uint32_t>(), std::declval<long>(), std::declval<long>(), std::declval<uint64_t>(), std::declval<uint64_t>(), std::declval<uint64_t>(), std::declval<uint32_t>(), std::declval<uint8_t>()));

      template <typename T>
      using has_final_stats = decltype(std::declval<T>().replacement_final_stats());
    }

    template <typename T>
    constexpr bool has_initialize()
    {
      return champsim::is_detected_v<detail::has_initialize, T>;
    }

    template <typename T>
    constexpr bool has_update_state()
    {
      return champsim::is_detected_v<detail::has_update_state, T>;
    }

    template <typename T>
    constexpr bool has_final_stats()
    {
      return champsim::is_detected_v<detail::has_final_stats, T>;
    }
  }
}

#endif
