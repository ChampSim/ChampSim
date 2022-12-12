#ifndef MSL_BITS_H
#define MSL_BITS_H

#include <cstdint>
#include <limits>

namespace champsim::msl
{
constexpr unsigned lg2(uint64_t n) { return n < 2 ? 0 : 1 + lg2(n / 2); }

constexpr uint64_t bitmask(std::size_t begin, std::size_t end = 0)
{
  return (begin - end < 64) ? ((1ull << (begin - end)) - 1) << end : std::numeric_limits<uint64_t>::max();
}

constexpr uint64_t splice_bits(uint64_t upper, uint64_t lower, std::size_t bits_upper, std::size_t bits_lower) { return (upper & ~bitmask(bits_upper, bits_lower)) | (lower & bitmask(bits_upper, bits_lower)); }
constexpr uint64_t splice_bits(uint64_t upper, uint64_t lower, std::size_t bits) { return splice_bits(upper, lower, bits, 0); }
} // namespace champsim::msl

#endif
