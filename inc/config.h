#ifndef CONFIG_H
#define CONFIG_H

#include <string_view>

namespace champsim::config {
  long int_or_prefixed_size(long val);
  long int_or_prefixed_size(std::string_view val);
}

#endif
