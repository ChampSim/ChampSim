#ifndef CONFIG_H
#define CONFIG_H

#include <string_view>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace champsim::config {
  long int_or_prefixed_size(long val);
  long int_or_prefixed_size(std::string_view val);

  std::map<std::string, long> propogate(std::map<std::string, long> from, std::map<std::string, long> to, std::string key);
  std::vector<std::map<std::string, long>> propogate_down(std::vector<std::map<std::string, long>> population, std::string key);
}

#endif
