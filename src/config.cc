#include "config.h"

#include <algorithm>
#include <charconv>
#include <map>

long champsim::config::int_or_prefixed_size(long val)
{
  return val;
}

long champsim::config::int_or_prefixed_size(std::string_view val)
{
  using namespace std::literals::string_view_literals;
  std::array<std::pair<std::string_view, long>, 13> multipliers{
    std::pair{"B"sv, 1},
    std::pair{"k"sv, 1024}, std::pair{"kB"sv, 1024}, std::pair{"kiB"sv, 1024},
    std::pair{"M"sv, 1024l*1024}, std::pair{"MB"sv, 1024l*1024}, std::pair{"MiB"sv, 1024l*1024},
    std::pair{"G"sv, 1024l*1024*1024}, std::pair{"GB"sv, 1024l*1024*1024}, std::pair{"GiB"sv, 1024l*1024*1024},
    std::pair{"T"sv, 1024l*1024*1024*1024}, std::pair{"TB"sv, 1024l*1024*1024*1024}, std::pair{"TiB"sv, 1024l*1024*1024*1024}
  };

  long result = 0;
  auto [ptr, ec] = std::from_chars(std::begin(val), std::end(val), result);
  if (ptr == std::end(val)) {
    return result;
  }

  std::size_t suffix_length = static_cast<std::size_t>(std::distance(ptr, std::end(val)));
  std::string_view suffix{ptr, suffix_length};
  auto found_multiplier = std::find_if(std::begin(multipliers), std::end(multipliers), [suffix](auto x){ return x.first == suffix; })->second;
  return result*found_multiplier;
}
