#ifndef PHASE_INFO_H
#define PHASE_INFO_H

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace champsim {

struct phase_info {
  std::string name;
  bool is_warmup;
  uint64_t length;
  std::vector<std::string> trace_names;
};

}

#endif

