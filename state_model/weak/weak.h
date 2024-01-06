#ifndef REPLACEMENT_WEAK_H
#define REPLACEMENT_WEAK_H

#include <vector>

#include "cache.h"
#include "modules.h"

struct weak : champsim::modules::replacement {
  uint64_t cycle = 0;

  explicit weak(CACHE* cache);
  weak(CACHE* cache, long sets, long ways);

};

#endif
