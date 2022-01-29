#ifndef CHAMPSIM_H
#define CHAMPSIM_H

#include <array>
#include <cstdint>
#include <exception>
#include <iostream>

#include "champsim_constants.h"

// USEFUL MACROS
//#define DEBUG_PRINT
#define SANITY_CHECK
#define LLC_BYPASS
#define DRC_BYPASS
#define NO_CRC2_COMPILE

#ifdef DEBUG_PRINT
#define DP(x) x
#else
#define DP(x)
#endif

// CACHE
#define INFLIGHT 1
#define COMPLETED 2

#define FILL_L1 1
#define FILL_L2 2
#define FILL_LLC 4
#define FILL_DRC 8
#define FILL_DRAM 16

using namespace std;

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
} // namespace champsim

extern champsim::deprecated_clock_cycle current_core_cycle;

#endif
