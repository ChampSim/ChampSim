#include "extent.h"

namespace
{
template <typename T>
auto size(const T& ext)
{
  return champsim::to_underlying(ext.upper) - champsim::to_underlying(ext.lower);
}
} // namespace

std::size_t champsim::size(dynamic_extent ext) { return ::size(ext); }

std::size_t champsim::size(sized_extent ext) { return ::size(ext); }

std::size_t champsim::size(page_offset_extent ext) { return ::size(ext); }

std::size_t champsim::size(page_number_extent ext) { return ::size(ext); }

std::size_t champsim::size(block_offset_extent ext) { return ::size(ext); }

std::size_t champsim::size(block_number_extent ext) { return ::size(ext); }
