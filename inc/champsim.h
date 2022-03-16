#ifndef CHAMPSIM_H
#define CHAMPSIM_H

#include <array>
#include <cstdint>
#include <exception>
#include <iostream>

#include "champsim_constants.h"

#define INFLIGHT 1
#define COMPLETED 2

#define FILL_L1 1
#define FILL_L2 2
#define FILL_LLC 4
#define FILL_DRC 8
#define FILL_DRAM 16

extern uint8_t warmup_complete[NUM_CPUS];

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
