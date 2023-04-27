#ifndef BLOCK_H
#define BLOCK_H

#include "champsim.h"

namespace champsim {
struct cache_block {
  bool valid = false;
  bool prefetch = false;
  bool dirty = false;

  champsim::address address{};
  champsim::address v_address{};
  champsim::address data{};

  uint32_t pf_metadata = 0;
};
}

#endif
