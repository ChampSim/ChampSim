#ifndef CHAMPSIM_H
#define CHAMPSIM_H

#include <cstdint>
#include <exception>

namespace champsim
{
struct deadlock : public std::exception {
  const uint32_t which;
  explicit deadlock(uint32_t cpu) : which(cpu) {}
};

struct deprecated_clock_cycle {
  uint64_t operator[](std::size_t cpu_idx);
};

#ifdef DEBUG_PRINT
constexpr bool debug_print = true;
#else
constexpr bool debug_print = false;
#endif
} // namespace champsim

extern champsim::deprecated_clock_cycle current_core_cycle;

#endif
